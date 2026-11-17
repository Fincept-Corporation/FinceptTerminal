// RunSchedulesDialog.cpp — the list and editor of scheduled runs (WP6). See the
// header. The rules (occurrences, missed runs, overlap, daily cap) live in
// TaskRunScheduler / ScheduleLogic; this dialog edits and shows them.

#include "screens/chat_mode/RunSchedulesDialog.h"

#include "screens/chat_mode/RunRequestForm.h"
#include "screens/chat_mode/RunViewLogic.h"
#include "screens/chat_mode/RunViewWidgets.h"
#include "services/agent_tasks/RunTemplateLibrary.h"
#include "services/agent_tasks/ScheduleLogic.h"
#include "services/agent_tasks/TaskRunScheduler.h"
#include "services/agent_tasks/TaskRunService.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/EmptyState.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDateTimeEdit>
#include <QDoubleSpinBox>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QLocale>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QSplitter>
#include <QStackedWidget>
#include <QStyle>
#include <QTableWidget>
#include <QTimeEdit>
#include <QTimeZone>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

namespace fincept::chat_mode {

namespace agt = fincept::agent_tasks;
namespace rv = fincept::chat_mode::run_view;
namespace sl = fincept::agent_tasks::schedule_logic;

namespace run_schedules_dialog_detail {

constexpr int kRsdIdRole = Qt::UserRole + 1;
constexpr int kRsdRefreshMs = 30 * 1000;

enum RsdColumn { kRsdColOn = 0, kRsdColName, kRsdColWhen, kRsdColNext, kRsdColLast, kRsdColCount };

QString rsd_style() {
    return QStringLiteral(
               "#runSchedulesDialog{background:%1;}"
               "#runSchedulesDialog QLabel{background:transparent;border:none;color:%2;font-size:%3px;}"
               "#runSchedulesDialog QLabel#rsdSection{color:%4;font-weight:700;letter-spacing:1px;}"
               "#runSchedulesDialog QLabel#rsdProblems{color:%5;}"
               "#runSchedulesDialog QLabel#rsdHint{color:%4;}"
               "#runSchedulesDialog QFrame#rsdNote{background:%6;border:1px solid %7;}"
               "#runSchedulesDialog QFrame#rsdConfirm{background:%8;border:1px solid %5;}"
               "#runSchedulesDialog QTableWidget,#runSchedulesDialog QListWidget{background:%9;color:%10;"
               "border:1px solid %11;gridline-color:%11;font-size:%3px;outline:none;}")
               .arg(ui::colors::BG_SURFACE(), ui::colors::TEXT_SECONDARY(), ui::fonts::px(-2), ui::colors::TEXT_TERTIARY(),
                    ui::colors::DANGER(), ui::colors::tint(QColor(ui::colors::INFO()), 0.10), ui::colors::INFO(),
                    ui::colors::tint(QColor(ui::colors::DANGER()), 0.12), ui::colors::BG_BASE())
               .arg(ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_DIM()) +
           QStringLiteral(
               "#runSchedulesDialog QTableWidget::item:selected,#runSchedulesDialog QListWidget::item:selected"
               "{background:%1;color:%2;}"
               "#runSchedulesDialog QHeaderView::section{background:%3;color:%4;border:none;border-bottom:1px solid %5;"
               "padding:3px 6px;font-size:%6px;font-weight:700;}"
               "#runSchedulesDialog QPushButton,#runSchedulesDialog QToolButton{background:%3;color:%4;border:1px solid %5;"
               "padding:4px 12px;font-size:%6px;font-weight:700;}"
               "#runSchedulesDialog QPushButton:hover,#runSchedulesDialog QToolButton:hover{color:%2;border-color:%7;}"
               "#runSchedulesDialog QPushButton:disabled{color:%8;background:transparent;}"
               "#runSchedulesDialog QPushButton#rsdPrimary{background:%7;color:%9;border-color:%7;}"
               "#runSchedulesDialog QPushButton#rsdDanger{color:%10;border-color:%10;}"
               "#runSchedulesDialog QToolButton:checked{background:%7;color:%9;border-color:%7;}")
               .arg(ui::colors::BG_HOVER(), ui::colors::TEXT_PRIMARY(), ui::colors::BG_RAISED(), ui::colors::TEXT_SECONDARY(),
                    ui::colors::BORDER_DIM(), ui::fonts::px(-3), ui::colors::AMBER(), ui::colors::TEXT_DIM(),
                    ui::colors::TEXT_ON_ACCENT())
               .arg(ui::colors::DANGER()) +
           QStringLiteral(
               "#runSchedulesDialog QLineEdit,#runSchedulesDialog QComboBox,#runSchedulesDialog QSpinBox,"
               "#runSchedulesDialog QDoubleSpinBox,#runSchedulesDialog QTimeEdit,#runSchedulesDialog QDateTimeEdit"
               "{background:%1;color:%2;border:1px solid %3;padding:2px 6px;font-size:%4px;}"
               "#runSchedulesDialog QCheckBox{color:%5;font-size:%4px;}"
               "#runSchedulesDialog QScrollArea{background:transparent;border:none;}")
               .arg(ui::colors::BG_BASE(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_DIM(), ui::fonts::px(-2),
                    ui::colors::TEXT_SECONDARY());
}

QPushButton* rsd_button(QWidget* parent, const char* name = nullptr) {
    auto* b = new QPushButton(parent);
    b->setCursor(Qt::PointingHandCursor);
    b->setAutoDefault(false);
    if (name)
        b->setObjectName(QLatin1String(name));
    return b;
}

QLabel* rsd_label(QWidget* parent, const char* name = nullptr) {
    auto* l = new QLabel(parent);
    if (name)
        l->setObjectName(QLatin1String(name));
    l->setWordWrap(true);
    l->setTextFormat(Qt::PlainText);
    return l;
}

/// "today 08:00", "Tue 08:00", "20 Nov 09:00" — plus "(in 3h 05m)".
QString rsd_when(qint64 ms, const QTimeZone& zone, qint64 now) {
    if (ms <= 0)
        return RunSchedulesDialog::tr("—");
    const QDateTime at = QDateTime::fromMSecsSinceEpoch(ms, zone);
    const QDate today = QDateTime::fromMSecsSinceEpoch(now, zone).date();
    QString day;
    if (at.date() == today)
        day = RunSchedulesDialog::tr("today");
    else if (at.date() == today.addDays(1))
        day = RunSchedulesDialog::tr("tomorrow");
    else if (at.date() > today && at.date() < today.addDays(7))
        day = QLocale().dayName(at.date().dayOfWeek(), QLocale::ShortFormat);
    else
        day = QLocale().toString(at.date(), QStringLiteral("d MMM"));
    QString s = day + QLatin1Char(' ') + at.toString(QStringLiteral("HH:mm"));
    const qint64 in = ms - now;
    if (in > 0)
        s += QStringLiteral(" (") + RunSchedulesDialog::tr("in %1").arg(rv::format_duration(in)) + QLatin1Char(')');
    return s;
}

/// The schedule's last outcome as a short label and its tone.
QPair<QString, rv::Tone> rsd_last(const agt::RunSchedule& s) {
    if (s.paused_for_review)
        return {RunSchedulesDialog::tr("PAUSED FOR REVIEW"), rv::Tone::Danger};
    const QString st = s.last_status;
    if (st.isEmpty())
        return {RunSchedulesDialog::tr("not run yet"), rv::Tone::Neutral};
    if (st == QLatin1String("starting") || st == QLatin1String("started"))
        return {RunSchedulesDialog::tr("started"), rv::Tone::Info};
    if (st == QLatin1String("skipped_overlap"))
        return {RunSchedulesDialog::tr("skipped: previous still going"), rv::Tone::Warning};
    if (st == QLatin1String("skipped_cap"))
        return {RunSchedulesDialog::tr("skipped: daily credit cap"), rv::Tone::Warning};
    if (st == QLatin1String("missed"))
        return {RunSchedulesDialog::tr("missed (terminal closed)"), rv::Tone::Warning};
    if (st == QLatin1String("failed_to_start"))
        return {RunSchedulesDialog::tr("could not start"), rv::Tone::Danger};
    if (st == QLatin1String("waiting_login"))
        return {RunSchedulesDialog::tr("waiting for sign-in"), rv::Tone::Warning};
    if (const auto rs = agt::run_status_from_id(st)) {
        if (agt::needs_attention(*rs))
            return {RunSchedulesDialog::tr("NEEDS YOU"), rv::Tone::Warning};
        return {agt::run_status_label(*rs), rv::run_status_tone(*rs)};
    }
    return {st, rv::Tone::Neutral};
}

/// IANA zones offered besides the computer's own (only those this system knows).
QList<QByteArray> rsd_zones() {
    static const char* const kZones[] = {"UTC",           "America/New_York", "America/Chicago", "America/Los_Angeles",
                                         "America/Toronto", "America/Sao_Paulo", "Europe/London", "Europe/Paris",
                                         "Europe/Frankfurt", "Europe/Zurich",   "Asia/Dubai",    "Asia/Kolkata",
                                         "Asia/Singapore", "Asia/Hong_Kong",  "Asia/Shanghai", "Asia/Tokyo",
                                         "Australia/Sydney"};
    QList<QByteArray> out;
    for (const char* z : kZones)
        if (QTimeZone::isTimeZoneIdAvailable(QByteArray(z)))
            out << QByteArray(z);
    return out;
}

} // namespace run_schedules_dialog_detail

using namespace run_schedules_dialog_detail;

struct RunSchedulesDialog::Impl {
    RunSchedulesDialog* q = nullptr;
    QString editing_id; ///< the schedule in the editor; empty = a new one
    bool editor_dirty = false;
    bool filling = false;

    QFrame* note = nullptr;
    QLabel* note_text = nullptr;
    QDoubleSpinBox* cap = nullptr;
    QLabel* cap_today = nullptr;
    QTableWidget* table = nullptr;
    QPushButton* new_btn = nullptr;
    QPushButton* run_btn = nullptr;
    QPushButton* resume_btn = nullptr;
    QPushButton* delete_btn = nullptr;
    QFrame* confirm = nullptr;
    QLabel* confirm_text = nullptr;
    QPushButton* confirm_yes = nullptr;
    QPushButton* confirm_no = nullptr;
    QLabel* list_status = nullptr;

    QStackedWidget* right = nullptr; // 0 empty state, 1 editor
    ui::EmptyState* empty = nullptr;
    QLabel* editor_title = nullptr;
    QLabel* when_title = nullptr;
    QLineEdit* name = nullptr;
    QComboBox* tmpl = nullptr;
    RunRequestForm* form = nullptr;
    QComboBox* kind = nullptr;
    QTimeEdit* time = nullptr;
    QDateTimeEdit* once_at = nullptr;
    QWidget* days_box = nullptr;
    QList<QToolButton*> days;
    QSpinBox* hours = nullptr;
    QComboBox* zone = nullptr;
    QLabel* zone_now = nullptr;
    QSpinBox* catch_up = nullptr;
    QCheckBox* enabled = nullptr;
    QLabel* next_preview = nullptr;
    QLabel* problems = nullptr;
    QPushButton* save_btn = nullptr;
    QPushButton* revert_btn = nullptr;
    QLabel* runs_title = nullptr;
    QListWidget* runs = nullptr;
    QTimer* refresh_timer = nullptr;

    void build();
    void wire();
    void retranslate();
    void refresh_table();
    void update_buttons();
    void show_empty();
    void edit(const QString& id);
    void fill_templates();
    agt::RunSchedule editor_schedule() const;
    void update_recurrence_ui();
    void update_preview();
    void save();
    void refresh_runs();
    void list_message(const QString& text, bool error);
    QString selected_id() const {
        const int row = table->currentRow();
        return row >= 0 && table->item(row, kRsdColName) ? table->item(row, kRsdColName)->data(kRsdIdRole).toString() : QString();
    }
};

void RunSchedulesDialog::Impl::build() {
    q->setObjectName(QStringLiteral("runSchedulesDialog"));
    q->setStyleSheet(rsd_style()); // before the children exist
    q->setModal(false);
    auto* outer = new QVBoxLayout(q);
    outer->setContentsMargins(14, 12, 14, 12);
    outer->setSpacing(8);

    note = new QFrame(q);
    note->setObjectName(QStringLiteral("rsdNote"));
    auto* nl = new QHBoxLayout(note);
    nl->setContentsMargins(10, 6, 10, 6);
    note_text = rsd_label(note);
    nl->addWidget(note_text, 1);
    outer->addWidget(note);

    auto* split = new QSplitter(Qt::Horizontal, q);
    split->setChildrenCollapsible(false);

    // ── Left: the schedules ──
    auto* left = new QWidget(split);
    auto* ll = new QVBoxLayout(left);
    ll->setContentsMargins(0, 0, 0, 0);
    ll->setSpacing(6);
    auto* cap_row = new QHBoxLayout;
    auto* cap_lbl = rsd_label(left);
    cap_lbl->setObjectName(QStringLiteral("rsdCapLabel"));
    cap = new QDoubleSpinBox(left);
    cap->setRange(0, 100000);
    cap->setDecimals(0);
    cap->setSingleStep(10);
    cap_today = rsd_label(left, "rsdHint");
    cap_row->addWidget(cap_lbl);
    cap_row->addWidget(cap);
    cap_row->addWidget(cap_today, 1);
    ll->addLayout(cap_row);
    table = new QTableWidget(0, kRsdColCount, left);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->verticalHeader()->setVisible(false);
    table->horizontalHeader()->setStretchLastSection(true);
    table->horizontalHeader()->setSectionResizeMode(kRsdColName, QHeaderView::Stretch);
    table->setWordWrap(false);
    ll->addWidget(table, 1);
    auto* lb_host = new QWidget(left);
    auto* lb = new rv::RvFlowLayout(lb_host, 6, 4);
    new_btn = rsd_button(lb_host, "rsdPrimary");
    run_btn = rsd_button(lb_host);
    run_btn->setProperty("fincept_agent_risk", "spend"); // agents: runs the scheduled agent task now (model quota)
    resume_btn = rsd_button(lb_host);
    delete_btn = rsd_button(lb_host, "rsdDanger");
    delete_btn->setProperty("fincept_agent_risk", "delete"); // agents: deletes the schedule
    for (QPushButton* b : {new_btn, run_btn, resume_btn, delete_btn})
        lb->addWidget(b);
    ll->addWidget(lb_host);
    confirm = new QFrame(left);
    confirm->setObjectName(QStringLiteral("rsdConfirm"));
    auto* cl = new QVBoxLayout(confirm);
    cl->setContentsMargins(8, 6, 8, 6);
    confirm_text = rsd_label(confirm);
    cl->addWidget(confirm_text);
    auto* cr = new QHBoxLayout;
    confirm_yes = rsd_button(confirm, "rsdDanger");
    confirm_no = rsd_button(confirm);
    cr->addWidget(confirm_yes);
    cr->addWidget(confirm_no);
    cr->addStretch();
    cl->addLayout(cr);
    confirm->setVisible(false);
    ll->addWidget(confirm);
    list_status = rsd_label(left, "rsdHint");
    list_status->setVisible(false);
    ll->addWidget(list_status);
    split->addWidget(left);

    // ── Right: the editor ──
    right = new QStackedWidget(split);
    empty = new ui::EmptyState(right);
    right->addWidget(empty);
    auto* scroll = new QScrollArea(right);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* editor = new QWidget(scroll);
    auto* el = new QVBoxLayout(editor);
    el->setContentsMargins(0, 0, 6, 0);
    el->setSpacing(8);
    editor_title = rsd_label(editor, "rsdSection");
    el->addWidget(editor_title);
    auto* top = new QGridLayout;
    top->setHorizontalSpacing(10);
    top->setVerticalSpacing(6);
    auto* name_lbl = rsd_label(editor);
    name_lbl->setObjectName(QStringLiteral("rsdNameLabel"));
    name = new QLineEdit(editor);
    name->setMaxLength(120);
    auto* tmpl_lbl = rsd_label(editor);
    tmpl_lbl->setObjectName(QStringLiteral("rsdTemplateLabel"));
    tmpl = new QComboBox(editor);
    top->addWidget(name_lbl, 0, 0);
    top->addWidget(name, 0, 1);
    top->addWidget(tmpl_lbl, 1, 0);
    top->addWidget(tmpl, 1, 1);
    top->setColumnStretch(1, 1);
    el->addLayout(top);
    form = new RunRequestForm(editor, /*for_schedule=*/true);
    el->addWidget(form);

    when_title = rsd_label(editor, "rsdSection");
    el->addWidget(when_title);
    auto* when = new QGridLayout;
    when->setHorizontalSpacing(10);
    when->setVerticalSpacing(6);
    kind = new QComboBox(editor);
    for (const char* k : {agt::kScheduleDaily, agt::kScheduleWeekly, agt::kScheduleInterval, agt::kScheduleOnce})
        kind->addItem(QString(), QString::fromLatin1(k));
    time = new QTimeEdit(QTime(8, 0), editor);
    time->setDisplayFormat(QStringLiteral("HH:mm"));
    once_at = new QDateTimeEdit(QDateTime::currentDateTime().addSecs(3600), editor);
    once_at->setDisplayFormat(QStringLiteral("yyyy-MM-dd HH:mm"));
    once_at->setCalendarPopup(true);
    hours = new QSpinBox(editor);
    hours->setRange(sl::kMinIntervalMinutes / 60, sl::kMaxIntervalMinutes / 60);
    days_box = new QWidget(editor);
    auto* dl = new QHBoxLayout(days_box);
    dl->setContentsMargins(0, 0, 0, 0);
    dl->setSpacing(3);
    for (int i = 0; i < 7; ++i) {
        auto* b = new QToolButton(days_box);
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        dl->addWidget(b);
        days << b;
    }
    dl->addStretch();
    zone = new QComboBox(editor);
    zone->addItem(QString(), QString()); // the computer's zone
    for (const QByteArray& z : rsd_zones())
        zone->addItem(QString::fromUtf8(z), QString::fromUtf8(z));
    zone_now = rsd_label(editor, "rsdHint");
    catch_up = new QSpinBox(editor);
    catch_up->setRange(0, sl::kMaxCatchUpMinutes);
    catch_up->setSingleStep(30);
    enabled = new QCheckBox(editor);
    int row = 0;
    const auto add_row = [&](const char* label_name, QWidget* w) {
        auto* l = rsd_label(editor);
        l->setObjectName(QLatin1String(label_name));
        l->setWordWrap(false);
        when->addWidget(l, row, 0);
        when->addWidget(w, row, 1);
        ++row;
    };
    add_row("rsdKindLabel", kind);
    add_row("rsdTimeLabel", time);
    add_row("rsdOnceLabel", once_at);
    add_row("rsdHoursLabel", hours);
    add_row("rsdDaysLabel", days_box);
    add_row("rsdZoneLabel", zone);
    when->addWidget(zone_now, row++, 1);
    add_row("rsdCatchLabel", catch_up);
    when->addWidget(enabled, row++, 1);
    when->setColumnStretch(1, 1);
    el->addLayout(when);
    next_preview = rsd_label(editor, "rsdHint");
    el->addWidget(next_preview);
    problems = rsd_label(editor, "rsdProblems");
    problems->setVisible(false);
    el->addWidget(problems);
    auto* eb = new QHBoxLayout;
    save_btn = rsd_button(editor, "rsdPrimary");
    revert_btn = rsd_button(editor);
    eb->addStretch();
    eb->addWidget(revert_btn);
    eb->addWidget(save_btn);
    el->addLayout(eb);
    runs_title = rsd_label(editor, "rsdSection");
    el->addWidget(runs_title);
    runs = new QListWidget(editor);
    runs->setMaximumHeight(QFontMetrics(ui::fonts::font(-2)).lineSpacing() * 7 + 10);
    el->addWidget(runs);
    el->addStretch();
    scroll->setWidget(editor);
    right->addWidget(scroll);
    split->addWidget(right);
    split->setStretchFactor(0, 2);
    split->setStretchFactor(1, 3);
    outer->addWidget(split, 1);

    auto* close_row = new QHBoxLayout;
    close_row->addStretch();
    auto* close_btn = rsd_button(q);
    close_btn->setObjectName(QStringLiteral("rsdClose"));
    close_row->addWidget(close_btn);
    outer->addLayout(close_row);
    QObject::connect(close_btn, &QPushButton::clicked, q, [this]() { q->close(); });

    refresh_timer = new QTimer(q);
    refresh_timer->setInterval(kRsdRefreshMs);

    const QFontMetrics fm(ui::fonts::font(-2));
    q->resize(fm.averageCharWidth() * 150, fm.lineSpacing() * 44);
    q->setMinimumSize(fm.averageCharWidth() * 90, fm.lineSpacing() * 26);
}

void RunSchedulesDialog::Impl::wire() {
    auto& sched = agt::TaskRunScheduler::instance();
    QObject::connect(&sched, &agt::TaskRunScheduler::schedules_changed, q, [this]() {
        refresh_table();
        refresh_runs();
    });
    QObject::connect(&agt::TaskRunService::instance(), &agt::TaskRunService::run_updated, q, [this](const QString& id) {
        if (const agt::TaskRun* r = agt::TaskRunService::instance().find_run(id); r && r->schedule_id == editing_id)
            refresh_runs();
    });
    QObject::connect(&agt::RunTemplateLibrary::instance(), &agt::RunTemplateLibrary::templates_changed, q,
                     [this]() { fill_templates(); });
    QObject::connect(refresh_timer, &QTimer::timeout, q, [this]() { refresh_table(); });
    new rv::RvEventHook(q, [this](QEvent::Type t) {
        if (t == QEvent::Show)
            refresh_timer->start();
        else if (t == QEvent::Hide)
            refresh_timer->stop(); // no timer while hidden
        else if (t == QEvent::LanguageChange)
            retranslate();
    });
    QObject::connect(cap, &QDoubleSpinBox::editingFinished, q, [this]() {
        agt::TaskRunScheduler::instance().set_daily_credit_cap(cap->value());
    });
    QObject::connect(table, &QTableWidget::currentCellChanged, q, [this](int, int, int, int) {
        update_buttons();
        const QString id = selected_id();
        if (!id.isEmpty() && id != editing_id && !filling)
            edit(id);
    });
    QObject::connect(table, &QTableWidget::cellChanged, q, [this](int row, int column) {
        if (filling || column != kRsdColOn)
            return;
        QTableWidgetItem* on = table->item(row, kRsdColOn);
        QTableWidgetItem* nm = table->item(row, kRsdColName);
        if (!on || !nm)
            return;
        // Deferred: the change rebuilds the table, which must not delete the item
        // whose change is being delivered.
        const QString id = nm->data(kRsdIdRole).toString();
        const bool turn_on = on->checkState() == Qt::Checked;
        QTimer::singleShot(0, q, [id, turn_on]() { agt::TaskRunScheduler::instance().set_enabled(id, turn_on); });
    });
    QObject::connect(new_btn, &QPushButton::clicked, q, [this]() { edit(QString()); });
    QObject::connect(empty, &ui::EmptyState::primary_clicked, q, [this]() { edit(QString()); });
    QObject::connect(run_btn, &QPushButton::clicked, q, [this]() {
        const QString id = selected_id();
        if (id.isEmpty())
            return;
        run_btn->setEnabled(false);
        list_message(RunSchedulesDialog::tr("Starting…"), false);
        QPointer<RunSchedulesDialog> self(q);
        agt::TaskRunScheduler::instance().run_schedule_now(id, q, [self](bool ok, const QString& run_id, const QString& err) {
            if (!self)
                return;
            self->d_->update_buttons();
            self->d_->list_message(ok ? RunSchedulesDialog::tr("Started — follow it in RUNS.") : err, !ok);
            if (ok)
                emit self->run_started(run_id);
        });
    });
    QObject::connect(resume_btn, &QPushButton::clicked, q, [this]() {
        agt::TaskRunScheduler::instance().resume_after_review(selected_id());
    });
    QObject::connect(delete_btn, &QPushButton::clicked, q, [this]() {
        const agt::RunSchedule* s = agt::TaskRunScheduler::instance().find_schedule(selected_id());
        if (!s)
            return;
        confirm_text->setText(RunSchedulesDialog::tr("Delete the schedule “%1”? Runs it already started are kept.").arg(s->name));
        confirm->setVisible(true);
        confirm_yes->setFocus();
    });
    QObject::connect(confirm_no, &QPushButton::clicked, q, [this]() { confirm->setVisible(false); });
    QObject::connect(confirm_yes, &QPushButton::clicked, q, [this]() {
        const QString id = selected_id();
        confirm->setVisible(false);
        agt::TaskRunScheduler::instance().delete_schedule(id);
        if (id == editing_id)
            show_empty();
    });
    for (QPushButton* b : {confirm_yes, confirm_no})
        new rv::RvKeyHook(b, [this](const QKeyEvent* ke, bool apply) {
            if (ke->key() != Qt::Key_Escape)
                return false;
            if (apply)
                confirm->setVisible(false);
            return true;
        });

    const auto dirty = [this]() {
        if (filling)
            return;
        editor_dirty = true;
        update_recurrence_ui();
        update_preview();
    };
    QObject::connect(name, &QLineEdit::textEdited, q, dirty);
    QObject::connect(tmpl, &QComboBox::currentIndexChanged, q, [this, dirty]() {
        if (filling)
            return;
        const bool named_by_user = editor_dirty && !name->text().trimmed().isEmpty();
        form->set_template(tmpl->currentData().toString());
        if (!named_by_user) {
            const auto t = form->current_template();
            name->setText(t ? t->title : RunSchedulesDialog::tr("My scheduled task"));
        }
        dirty();
    });
    QObject::connect(form, &RunRequestForm::changed, q, dirty);
    QObject::connect(kind, &QComboBox::currentIndexChanged, q, dirty);
    QObject::connect(time, &QTimeEdit::timeChanged, q, dirty);
    QObject::connect(once_at, &QDateTimeEdit::dateTimeChanged, q, dirty);
    QObject::connect(hours, &QSpinBox::valueChanged, q, dirty);
    QObject::connect(zone, &QComboBox::currentIndexChanged, q, dirty);
    QObject::connect(catch_up, &QSpinBox::valueChanged, q, dirty);
    QObject::connect(enabled, &QCheckBox::toggled, q, dirty);
    for (QToolButton* b : std::as_const(days)) {
        QObject::connect(b, &QToolButton::toggled, q, [this, b, dirty]() {
            // A weekly schedule runs on one day: picking another moves it.
            if (!filling && b->isChecked() && kind->currentData().toString() == QLatin1String(agt::kScheduleWeekly)) {
                filling = true;
                for (QToolButton* o : std::as_const(days))
                    if (o != b)
                        o->setChecked(false);
                filling = false;
            }
            dirty();
        });
    }
    QObject::connect(save_btn, &QPushButton::clicked, q, [this]() { save(); });
    QObject::connect(revert_btn, &QPushButton::clicked, q, [this]() {
        if (editing_id.isEmpty())
            show_empty();
        else
            edit(editing_id);
    });
    QObject::connect(runs, &QListWidget::itemDoubleClicked, q, [this](QListWidgetItem* item) {
        const QString id = item ? item->data(kRsdIdRole).toString() : QString();
        if (!id.isEmpty())
            emit q->run_started(id);
    });
}

void RunSchedulesDialog::Impl::retranslate() {
    q->setWindowTitle(RunSchedulesDialog::tr("Scheduled runs"));
    const QTimeZone sys = QTimeZone::systemTimeZone();
    note_text->setText(RunSchedulesDialog::tr("Schedules run only while the terminal is open. A run missed while it was "
                                              "closed runs once when it opens again, if it is less late than the "
                                              "schedule's catch-up window; older ones are skipped. A schedule never "
                                              "overlaps its own previous run. Scheduled runs are unattended: trading "
                                              "tools are paper-only, and a run that needs you waits (\"needs you\") and "
                                              "notifies. This computer's time zone: %1.")
                           .arg(sl::zone_label(sys, QDateTime::currentMSecsSinceEpoch())));
    if (auto* l = q->findChild<QLabel*>(QStringLiteral("rsdCapLabel")))
        l->setText(RunSchedulesDialog::tr("Daily credit cap of scheduled runs"));
    cap->setSpecialValueText(RunSchedulesDialog::tr("no cap"));
    cap->setToolTip(RunSchedulesDialog::tr("All scheduled cloud runs of one day together spend at most this; a run that "
                                           "would go over is skipped and you are told"));
    table->setHorizontalHeaderLabels({RunSchedulesDialog::tr("On"), RunSchedulesDialog::tr("Name"),
                                      RunSchedulesDialog::tr("When"), RunSchedulesDialog::tr("Next run"),
                                      RunSchedulesDialog::tr("Last result")});
    new_btn->setText(RunSchedulesDialog::tr("NEW SCHEDULE"));
    run_btn->setText(RunSchedulesDialog::tr("RUN NOW"));
    resume_btn->setText(RunSchedulesDialog::tr("RESUME"));
    resume_btn->setToolTip(RunSchedulesDialog::tr("A run of this schedule failed or hit its budget, so it stopped: let it "
                                                  "run again"));
    delete_btn->setText(RunSchedulesDialog::tr("DELETE"));
    confirm_yes->setText(RunSchedulesDialog::tr("DELETE"));
    confirm_no->setText(RunSchedulesDialog::tr("KEEP"));
    const QStringList kinds = {RunSchedulesDialog::tr("Daily (on chosen days)"), RunSchedulesDialog::tr("Weekly"),
                               RunSchedulesDialog::tr("Every N hours"), RunSchedulesDialog::tr("Once")};
    for (int i = 0; i < kinds.size(); ++i)
        kind->setItemText(i, kinds.at(i));
    for (int i = 0; i < days.size(); ++i)
        days.at(i)->setText(QLocale().dayName(i + 1, QLocale::ShortFormat));
    zone->setItemText(0, RunSchedulesDialog::tr("This computer (%1)").arg(QString::fromUtf8(sys.id())));
    catch_up->setSpecialValueText(RunSchedulesDialog::tr("never catch up"));
    catch_up->setSuffix(RunSchedulesDialog::tr(" min"));
    hours->setSuffix(RunSchedulesDialog::tr(" h"));
    enabled->setText(RunSchedulesDialog::tr("Enabled"));
    const std::pair<const char*, QString> labels[] = {
        {"rsdNameLabel", RunSchedulesDialog::tr("Name")},
        {"rsdTemplateLabel", RunSchedulesDialog::tr("Task")},
        {"rsdKindLabel", RunSchedulesDialog::tr("Repeat")},
        {"rsdTimeLabel", RunSchedulesDialog::tr("At")},
        {"rsdOnceLabel", RunSchedulesDialog::tr("On")},
        {"rsdHoursLabel", RunSchedulesDialog::tr("Every")},
        {"rsdDaysLabel", RunSchedulesDialog::tr("Days")},
        {"rsdZoneLabel", RunSchedulesDialog::tr("Time zone")},
        {"rsdCatchLabel", RunSchedulesDialog::tr("Catch up within")}};
    when_title->setText(RunSchedulesDialog::tr("WHEN"));
    for (const auto& entry : labels)
        if (auto* l = q->findChild<QLabel*>(QLatin1String(entry.first)))
            l->setText(entry.second);
    catch_up->setToolTip(RunSchedulesDialog::tr("An occurrence missed while the terminal was closed runs once at start "
                                                "when it is at most this late"));
    save_btn->setText(RunSchedulesDialog::tr("SAVE SCHEDULE"));
    revert_btn->setText(RunSchedulesDialog::tr("DISCARD CHANGES"));
    runs_title->setText(RunSchedulesDialog::tr("RUNS OF THIS SCHEDULE (double-click to open)"));
    if (auto* b = q->findChild<QPushButton*>(QStringLiteral("rsdClose")))
        b->setText(RunSchedulesDialog::tr("CLOSE"));
    fill_templates();
    refresh_table();
}

void RunSchedulesDialog::Impl::fill_templates() {
    const QString keep = tmpl->currentData().toString();
    const bool was_filling = filling;
    filling = true;
    tmpl->clear();
    tmpl->addItem(RunSchedulesDialog::tr("Custom task"), QString());
    for (const agt::RunTemplate& t : agt::RunTemplateLibrary::instance().templates())
        tmpl->addItem(t.user_defined ? RunSchedulesDialog::tr("%1 (mine)").arg(t.title) : t.title, t.id);
    const int at = tmpl->findData(keep);
    tmpl->setCurrentIndex(at >= 0 ? at : 0);
    filling = was_filling;
}

void RunSchedulesDialog::Impl::list_message(const QString& text, bool error) {
    list_status->setText(text);
    list_status->setVisible(!text.isEmpty());
    const QString name_now = error ? QStringLiteral("rsdProblems") : QStringLiteral("rsdHint");
    if (list_status->objectName() != name_now) {
        list_status->setObjectName(name_now);
        list_status->style()->unpolish(list_status);
        list_status->style()->polish(list_status);
    }
}

void RunSchedulesDialog::Impl::refresh_table() {
    auto& sched = agt::TaskRunScheduler::instance();
    const QString keep = selected_id().isEmpty() ? editing_id : selected_id();
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const QVector<agt::RunSchedule> list = sched.schedules();
    const bool was_filling = filling;
    filling = true;
    table->setRowCount(static_cast<int>(list.size()));
    int select_row = -1;
    for (int i = 0; i < list.size(); ++i) {
        const agt::RunSchedule& s = list.at(i);
        auto* on = new QTableWidgetItem;
        on->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable | Qt::ItemIsSelectable);
        on->setCheckState(s.enabled ? Qt::Checked : Qt::Unchecked);
        on->setToolTip(s.enabled ? RunSchedulesDialog::tr("Runs on its schedule") : RunSchedulesDialog::tr("Off"));
        table->setItem(i, kRsdColOn, on);
        auto* nm = new QTableWidgetItem(s.name);
        nm->setData(kRsdIdRole, s.id);
        nm->setToolTip(s.name);
        table->setItem(i, kRsdColName, nm);
        const QTimeZone tz = sl::schedule_zone(s);
        auto* when = new QTableWidgetItem(sl::describe_recurrence(s));
        when->setToolTip(sl::describe_recurrence(s) + QLatin1Char('\n') + sl::zone_label(tz, now));
        table->setItem(i, kRsdColWhen, when);
        auto* next = new QTableWidgetItem(s.enabled && !s.paused_for_review ? rsd_when(s.next_run_at_ms, tz, now)
                                                                            : RunSchedulesDialog::tr("—"));
        table->setItem(i, kRsdColNext, next);
        const auto last = rsd_last(s);
        auto* lr = new QTableWidgetItem(last.first);
        lr->setForeground(rv::tone_color(last.second));
        QString tip = s.last_run_at_ms > 0 ? RunSchedulesDialog::tr("Last run %1").arg(rsd_when(s.last_run_at_ms, tz, now))
                                           : QString();
        if (!s.last_error.isEmpty())
            tip += (tip.isEmpty() ? QString() : QStringLiteral("\n")) + s.last_error;
        lr->setToolTip(tip);
        table->setItem(i, kRsdColLast, lr);
        if (s.id == keep)
            select_row = i;
    }
    if (select_row >= 0)
        table->setCurrentCell(select_row, kRsdColName);
    filling = was_filling;
    const QFontMetrics fm(table->font());
    table->setColumnWidth(kRsdColOn, fm.horizontalAdvance(QStringLiteral("OnXX")) + 24);
    table->setColumnWidth(kRsdColWhen, fm.averageCharWidth() * 22);
    table->setColumnWidth(kRsdColNext, fm.averageCharWidth() * 20);
    {
        const QSignalBlocker block(cap);
        cap->setValue(sched.daily_credit_cap());
    }
    cap_today->setText(RunSchedulesDialog::tr("today: %1 credits spent or reserved")
                           .arg(rv::format_credits(sched.committed_today())));
    update_buttons();
}

void RunSchedulesDialog::Impl::update_buttons() {
    const agt::RunSchedule* s = agt::TaskRunScheduler::instance().find_schedule(selected_id());
    const bool active = agt::TaskRunScheduler::instance().is_active();
    run_btn->setEnabled(s != nullptr && active);
    run_btn->setToolTip(!s        ? RunSchedulesDialog::tr("Select a schedule")
                        : !active ? RunSchedulesDialog::tr("This is a test run of the terminal: nothing starts")
                                  : RunSchedulesDialog::tr("Start its run now (the next scheduled one stays)"));
    resume_btn->setVisible(s && s->paused_for_review);
    delete_btn->setEnabled(s != nullptr);
}

void RunSchedulesDialog::Impl::show_empty() {
    editing_id.clear();
    editor_dirty = false;
    ui::EmptyState::Spec spec;
    spec.glyph = QStringLiteral("◷");
    const bool none = agt::TaskRunScheduler::instance().schedules().isEmpty();
    spec.title = none ? RunSchedulesDialog::tr("No schedules yet") : RunSchedulesDialog::tr("Select a schedule");
    spec.message = RunSchedulesDialog::tr("Run a task on its own — a market wrap every weekday at 07:30 New York time, a "
                                          "risk check of your portfolio every Monday, a news digest every 4 hours.");
    spec.primary = RunSchedulesDialog::tr("NEW SCHEDULE");
    spec.note = RunSchedulesDialog::tr("Only while the terminal is open");
    spec.note_tone = ui::EmptyState::Tone::Info;
    empty->apply(spec);
    right->setCurrentIndex(0);
}

void RunSchedulesDialog::Impl::edit(const QString& id) {
    confirm->setVisible(false);
    const agt::RunSchedule* existing = id.isEmpty() ? nullptr : agt::TaskRunScheduler::instance().find_schedule(id);
    agt::RunSchedule s;
    if (existing) {
        s = *existing;
    } else {
        s.kind = QString::fromLatin1(agt::kScheduleDaily);
        s.daily_time = QStringLiteral("08:00");
        s.weekdays_mask = 0x1f;
        s.template_id = QStringLiteral("market_wrap");
        s.request.plan_first = true;
    }
    editing_id = existing ? id : QString();
    filling = true;
    editor_title->setText(existing ? RunSchedulesDialog::tr("EDIT SCHEDULE") : RunSchedulesDialog::tr("NEW SCHEDULE"));
    name->setText(s.name);
    const int t_at = tmpl->findData(s.template_id);
    tmpl->setCurrentIndex(t_at >= 0 ? t_at : 0);
    // The run part through the shared form.
    QJsonObject v;
    v[QStringLiteral("template_id")] = t_at > 0 ? s.template_id : QString();
    v[QStringLiteral("params")] = s.request.context;
    const QString own_prompt = !s.prompt.isEmpty() ? s.prompt : (t_at <= 0 ? s.request.prompt : QString());
    v[QStringLiteral("prompt")] = own_prompt;
    v[QStringLiteral("prompt_edited")] = !own_prompt.isEmpty() && t_at > 0;
    v[QStringLiteral("engine")] = RunRequestForm::engine_value(s.engine, s.request.plan_first);
    v[QStringLiteral("mode")] = s.mode.isEmpty() ? s.request.mode : s.mode;
    if (existing) {
        QJsonObject b;
        b[QStringLiteral("max_credits")] = s.max_credits > 0 ? s.max_credits : s.request.budget.max_credits;
        b[QStringLiteral("max_steps")] = s.request.budget.max_steps;
        b[QStringLiteral("max_minutes")] = s.request.budget.max_minutes;
        v[QStringLiteral("budget")] = b;
    }
    form->set_values(v);
    if (s.name.isEmpty()) {
        const auto t = form->current_template();
        name->setText(t ? t->title : RunSchedulesDialog::tr("My scheduled task"));
    }
    kind->setCurrentIndex(std::max(0, kind->findData(s.kind)));
    const int minutes = sl::parse_daily_time(s.daily_time);
    time->setTime(minutes >= 0 ? QTime(minutes / 60, minutes % 60) : QTime(8, 0));
    hours->setValue(std::max(1, s.interval_minutes / 60));
    if (s.once_at_ms > 0) {
        // The editor shows wall-clock time in the schedule's zone (as it is entered).
        const QDateTime z = QDateTime::fromMSecsSinceEpoch(s.once_at_ms, sl::schedule_zone(s));
        once_at->setDateTime(QDateTime(z.date(), z.time()));
    } else {
        once_at->setDateTime(QDateTime::currentDateTime().addSecs(3600));
    }
    for (int i = 0; i < days.size(); ++i)
        days.at(i)->setChecked((s.weekdays_mask & (1 << i)) != 0);
    const int z_at = zone->findData(s.time_zone);
    zone->setCurrentIndex(z_at >= 0 ? z_at : 0);
    catch_up->setValue(std::clamp(s.catch_up_minutes, 0, sl::kMaxCatchUpMinutes));
    enabled->setChecked(existing ? s.enabled : true);
    filling = false;
    editor_dirty = !existing;
    update_recurrence_ui();
    update_preview();
    refresh_runs();
    right->setCurrentIndex(1);
}

agt::RunSchedule RunSchedulesDialog::Impl::editor_schedule() const {
    agt::RunSchedule s;
    if (const agt::RunSchedule* e = agt::TaskRunScheduler::instance().find_schedule(editing_id))
        s = *e;
    s.id = editing_id;
    s.name = name->text().trimmed();
    const agt::RunRequest r = form->request();
    s.template_id = form->template_id();
    // A schedule that follows its template keeps no text of its own: it is filled
    // again (periods, holdings) every time it runs.
    s.prompt = form->refill_template_id().isEmpty() ? r.prompt : QString();
    s.request = r;
    s.request.origin = QString::fromLatin1(agt::kOriginSchedule);
    s.engine = r.engine;
    s.mode = r.mode;
    s.max_credits = r.budget.max_credits;
    s.kind = kind->currentData().toString();
    s.daily_time = time->time().toString(QStringLiteral("HH:mm"));
    s.interval_minutes = hours->value() * 60;
    int mask = 0;
    for (int i = 0; i < days.size(); ++i)
        if (days.at(i)->isChecked())
            mask |= 1 << i;
    if (s.kind == QLatin1String(agt::kScheduleOnce))
        mask = 0x7f;
    s.weekdays_mask = mask;
    s.time_zone = zone->currentData().toString();
    // "Once" is entered as a wall-clock time in the chosen zone.
    const QDateTime local = once_at->dateTime();
    s.once_at_ms = QDateTime(local.date(), local.time(), sl::schedule_zone(s)).toMSecsSinceEpoch();
    s.catch_up_minutes = catch_up->value();
    s.enabled = enabled->isChecked();
    return s;
}

void RunSchedulesDialog::Impl::update_recurrence_ui() {
    const QString k = kind->currentData().toString();
    const bool once = k == QLatin1String(agt::kScheduleOnce);
    const bool interval = k == QLatin1String(agt::kScheduleInterval);
    const auto show_row = [this](const char* label, QWidget* w, bool visible) {
        w->setVisible(visible);
        if (auto* l = q->findChild<QLabel*>(QLatin1String(label)))
            l->setVisible(visible);
    };
    show_row("rsdTimeLabel", time, !once && !interval);
    show_row("rsdOnceLabel", once_at, once);
    show_row("rsdHoursLabel", hours, interval);
    show_row("rsdDaysLabel", days_box, !once);
    const agt::RunSchedule s = editor_schedule();
    const QTimeZone tz = sl::schedule_zone(s);
    zone_now->setText(RunSchedulesDialog::tr("Times are %1 — it is %2 there now.")
                          .arg(sl::zone_label(tz, QDateTime::currentMSecsSinceEpoch()),
                               QDateTime::currentDateTime().toTimeZone(tz).toString(QStringLiteral("HH:mm"))));
}

void RunSchedulesDialog::Impl::update_preview() {
    const agt::RunSchedule s = editor_schedule();
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    QStringList problems_list = sl::validate_schedule(s, now);
    problems_list += form->problems();
    problems_list.removeDuplicates();
    problems->setText(problems_list.isEmpty() ? QString() : QStringLiteral("• ") + problems_list.join(QStringLiteral("\n• ")));
    problems->setVisible(!problems_list.isEmpty());
    save_btn->setEnabled(problems_list.isEmpty());
    save_btn->setToolTip(problems_list.isEmpty() ? RunSchedulesDialog::tr("Save; it runs from its next occurrence")
                                                 : RunSchedulesDialog::tr("Fix what is listed first"));
    if (!problems_list.isEmpty()) {
        next_preview->setText(QString());
        return;
    }
    const QTimeZone tz = sl::schedule_zone(s);
    QStringList next;
    qint64 t = now;
    for (int i = 0; i < 3; ++i) {
        t = sl::next_occurrence_ms(s, t);
        if (t <= 0)
            break;
        next << rsd_when(t, tz, now);
    }
    QString text = next.isEmpty() ? RunSchedulesDialog::tr("No future run.")
                                  : RunSchedulesDialog::tr("Next runs: %1").arg(next.join(QStringLiteral(" · ")));
    const rv::BudgetConfirmation c = form->confirmation(-1);
    text += QLatin1Char('\n') + RunSchedulesDialog::tr("Each run: %1. The daily cap of scheduled runs still applies.")
                                    .arg(c.summary);
    next_preview->setText(text);
}

void RunSchedulesDialog::Impl::save() {
    QString err;
    const QString id = agt::TaskRunScheduler::instance().save_schedule(editor_schedule(), &err);
    if (id.isEmpty()) {
        problems->setText(err);
        problems->setVisible(true);
        return;
    }
    editing_id = id;
    editor_dirty = false;
    editor_title->setText(RunSchedulesDialog::tr("EDIT SCHEDULE"));
    refresh_table();
    refresh_runs();
    list_message(RunSchedulesDialog::tr("Schedule saved."), false);
}

void RunSchedulesDialog::Impl::refresh_runs() {
    runs->clear();
    runs_title->setVisible(!editing_id.isEmpty());
    runs->setVisible(!editing_id.isEmpty());
    if (editing_id.isEmpty())
        return;
    auto& svc = agt::TaskRunService::instance();
    const QStringList ids = agt::TaskRunScheduler::instance().runs_of(editing_id);
    if (ids.isEmpty()) {
        auto* item = new QListWidgetItem(RunSchedulesDialog::tr("No runs yet — RUN NOW starts one."), runs);
        item->setFlags(Qt::NoItemFlags);
        return;
    }
    for (const QString& id : ids) {
        const agt::TaskRun* r = svc.find_run(id);
        if (!r)
            continue;
        QStringList parts;
        parts << rv::run_status_pill(r->status);
        parts << QLocale().toString(QDateTime::fromMSecsSinceEpoch(r->created_at_ms), QLocale::ShortFormat);
        if (rv::run_uses_credits(*r))
            parts << rv::credits_text(*r, RunSchedulesDialog::tr("%1 credits").arg(rv::format_credits(r->credits_spent)));
        auto* item = new QListWidgetItem(parts.join(QStringLiteral(" · ")), runs);
        item->setData(kRsdIdRole, id);
        item->setForeground(rv::tone_color(rv::run_status_tone(r->status)));
        item->setToolTip(r->error.isEmpty() ? r->title : r->title + QLatin1Char('\n') + r->error);
    }
}

// ── RunSchedulesDialog ───────────────────────────────────────────────────────

RunSchedulesDialog::RunSchedulesDialog(QWidget* parent) : QDialog(parent), d_(std::make_unique<Impl>()) {
    d_->q = this;
    d_->build();
    d_->wire();
    agt::RunTemplateLibrary::instance().ensure_loaded();
    d_->retranslate();
    const QVector<agt::RunSchedule> list = agt::TaskRunScheduler::instance().schedules();
    if (list.isEmpty()) {
        d_->show_empty();
    } else {
        d_->edit(list.first().id); // never a blank editor
        d_->refresh_table();
    }
}

RunSchedulesDialog::~RunSchedulesDialog() = default;

void RunSchedulesDialog::start_new_schedule(const QJsonObject& draft) {
    d_->edit(QString());
    d_->filling = true;
    const QJsonObject form_values = draft.value(QStringLiteral("form")).toObject();
    const QString template_id = form_values.value(QStringLiteral("template_id")).toString();
    const int t_at = d_->tmpl->findData(template_id);
    d_->tmpl->setCurrentIndex(t_at >= 0 ? t_at : 0);
    d_->form->set_values(form_values);
    const QString name = draft.value(QStringLiteral("name")).toString().trimmed();
    if (!name.isEmpty())
        d_->name->setText(name.left(120));
    d_->filling = false;
    d_->editor_dirty = true;
    d_->update_recurrence_ui();
    d_->update_preview();
    d_->name->setFocus();
}

} // namespace fincept::chat_mode
