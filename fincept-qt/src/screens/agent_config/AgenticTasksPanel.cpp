// src/screens/agent_config/AgenticTasksPanel.cpp
#include "screens/agent_config/AgenticTasksPanel.h"

#include "core/logging/Logger.h"
#include "screens/agent_config/AgentResultFormat.h"
#include "services/agents/AgentService.h"
#include "ui/formatting/DateFormat.h"
#include "ui/markdown/MarkdownRenderer.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/EmptyState.h"
#include "ui/widgets/LoadingSkeleton.h"

#include <QComboBox>
#include <QCoreApplication>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollBar>
#include <QShowEvent>
#include <QSplitter>
#include <QStackedWidget>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTextBrowser>
#include <QTimeZone>
#include <QTimer>
#include <QVBoxLayout>

#include <memory>

namespace fincept::screens {

namespace {
constexpr int kTaskIdRole = Qt::UserRole + 1;
constexpr int kTaskJsonRole = Qt::UserRole + 2;

QString trunc(const QString& s, int n) {
    return s.length() <= n ? s : s.left(n - 1) + QStringLiteral("…");
}

/// The agent_tasks status a live event implies — the runner writes it before
/// emitting the event (control_ack carries the signal the C++ side just set).
/// Empty for events that do not change the status.
QString agentic_status_after_event(const QString& kind, const QJsonObject& event) {
    if (kind == QLatin1String("task_started") || kind == QLatin1String("task_resumed"))
        return QStringLiteral("running");
    if (kind == QLatin1String("done"))
        return QStringLiteral("completed");
    if (kind == QLatin1String("error"))
        return QStringLiteral("failed");
    if (kind == QLatin1String("cancelled"))
        return QStringLiteral("cancelled");
    if (kind == QLatin1String("paused"))
        return QStringLiteral("paused");
    if (kind == QLatin1String("question"))
        return QStringLiteral("paused_for_input");
    if (kind == QLatin1String("budget_stop"))
        return QStringLiteral("failed_budget");
    if (kind == QLatin1String("control_ack"))
        return event.value(QStringLiteral("signal")).toString(); // pause_requested / cancel_requested / cancelled
    return {};
}
} // namespace

AgenticTasksPanel::AgenticTasksPanel(QWidget* parent) : QWidget(parent) {
    setObjectName("AgenticTasksPanel");
    build_ui();
    wire_service();

    // Task rows carry their status colour as an item brush; the pill and the
    // rendered views carry theme colours too. Re-applied once an appearance
    // change has settled (the pill sets a stylesheet and shows / hides — never
    // inside a theme_changed handler).
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::appearance_settled, this, [this]() {
        for (int i = 0; i < task_list_->count(); ++i) {
            QListWidgetItem* item = task_list_->item(i);
            const QString status = item->data(kTaskJsonRole).toJsonObject().value("status").toString();
            item->setForeground(QColor(status_color_for(status)));
        }
        // Rendered answers, the step log and the plan glyphs carry theme colours.
        render_step_log();
        agent_result::rerender(final_view_);
        if (!shown_plan_.isEmpty())
            render_plan(shown_plan_);
        set_status_pill(shown_status_);
    });
}

namespace {
/// Colour of a step-log / plan state.
QString agentic_state_color(const QString& state) {
    if (state == QLatin1String("running"))
        return ui::colors::AMBER();
    if (state == QLatin1String("done") || state == QLatin1String("completed"))
        return ui::colors::SUCCESS();
    if (state == QLatin1String("error") || state == QLatin1String("failed"))
        return ui::colors::DANGER();
    if (state == QLatin1String("warn"))
        return ui::colors::WARNING();
    return ui::colors::TEXT_SECONDARY();
}

QString agentic_state_glyph(const QString& state) {
    if (state == QLatin1String("running"))
        return QStringLiteral("▶");
    if (state == QLatin1String("done") || state == QLatin1String("completed"))
        return QStringLiteral("✓");
    if (state == QLatin1String("error") || state == QLatin1String("failed"))
        return QStringLiteral("✕");
    return QStringLiteral("·");
}

/// "2026-11-16T10:00:00.123456" (naive UTC, Python utcnow().isoformat()) in the display zone.
QString agentic_created_text(const QString& iso) {
    QDateTime dt = QDateTime::fromString(iso, Qt::ISODateWithMs);
    if (!dt.isValid())
        dt = QDateTime::fromString(iso.left(19), Qt::ISODate);
    if (!dt.isValid())
        return iso.left(19);
    if (dt.timeSpec() == Qt::LocalTime)
        dt.setTimeZone(QTimeZone::UTC);
    return ui::formatting::date_time(dt, ui::formatting::DateStyle::Medium, false);
}
} // namespace

void AgenticTasksPanel::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(12, 12, 12, 12);
    root->setSpacing(8);

    // ── Filter bar ───────────────────────────────────────────────────────────
    auto* filter_bar = new QHBoxLayout;
    filter_bar->setSpacing(8);

    filter_title_ = new QLabel(tr("FILTER:"));
    filter_title_->setStyleSheet(
        QString("color:%1;font-size:" + fincept::ui::fonts::px(-4) + "px;letter-spacing:1px;").arg(ui::colors::TEXT_TERTIARY()));
    filter_bar->addWidget(filter_title_);

    filter_combo_ = new QComboBox;
    filter_combo_->addItem(tr("All"), QString());
    filter_combo_->addItem(tr("Running"), "running");
    filter_combo_->addItem(tr("Paused"), "paused");
    filter_combo_->addItem(tr("Completed"), "completed");
    filter_combo_->addItem(tr("Failed"), "failed");
    filter_combo_->addItem(tr("Cancelled"), "cancelled");
    filter_combo_->addItem(tr("Needs input"), "paused_for_input");
    filter_combo_->addItem(tr("Budget stop"), "failed_budget");
    filter_combo_->setStyleSheet(
        QString("QComboBox { background:%1; color:%2; border:1px solid %3; padding:3px 8px; font-size:" + fincept::ui::fonts::px(-3) + "px; }")
            .arg(ui::colors::BG_SURFACE(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_DIM()));
    filter_bar->addWidget(filter_combo_);

    refresh_btn_ = new QPushButton(tr("REFRESH"));
    refresh_btn_->setCursor(Qt::PointingHandCursor);
    refresh_btn_->setStyleSheet(
        QString("QPushButton { background:%1; color:%2; border:1px solid %3; padding:4px 12px; font-size:" + fincept::ui::fonts::px(-4) + "px; "
                "font-weight:600; letter-spacing:1px; }"
                "QPushButton:hover { color:%4; border-color:%4; }")
            .arg(ui::colors::BG_SURFACE(), ui::colors::TEXT_SECONDARY(), ui::colors::BORDER_DIM(),
                 ui::colors::AMBER()));
    filter_bar->addWidget(refresh_btn_);
    filter_bar->addStretch();
    root->addLayout(filter_bar);

    // ── Body: list (left) | detail (right) ───────────────────────────────────
    auto* splitter = new QSplitter(Qt::Horizontal);
    splitter->setHandleWidth(4);

    // The task list, or — with none — what fills it and how to start one.
    list_stack_ = new QStackedWidget;
    task_list_ = new QListWidget(list_stack_);
    task_list_->setStyleSheet(QString("QListWidget { background:%1; color:%2; border:1px solid %3; font-size:" + fincept::ui::fonts::px(-3) + "px; }"
                                      "QListWidget::item { padding:6px 8px; border-bottom:1px solid %4; }"
                                      "QListWidget::item:selected { background:%5; color:%6; }")
                                  .arg(ui::colors::BG_SURFACE(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_DIM(),
                                       ui::colors::BORDER_DIM(), ui::colors::BG_RAISED(), ui::colors::AMBER()));
    list_stack_->addWidget(task_list_);
    list_empty_ = new ui::EmptyState(list_stack_);
    list_empty_->set_compact(true);
    list_empty_->set_alignment(Qt::AlignTop);
    connect(list_empty_, &ui::EmptyState::primary_clicked, this, [this]() { open_schedule_dialog({}, {}, {}); });
    list_stack_->addWidget(list_empty_);
    splitter->addWidget(list_stack_);

    // Detail pane
    auto* detail = new QWidget;
    auto* dv = new QVBoxLayout(detail);
    dv->setContentsMargins(8, 0, 0, 0);
    dv->setSpacing(6);

    detail_header_ = new QLabel(tr("Select a task to view details"));
    detail_header_->setWordWrap(true);
    detail_header_->setStyleSheet(QString("color:%1;font-size:" + fincept::ui::fonts::px(-1) + "px;font-weight:600;").arg(ui::colors::AMBER()));
    dv->addWidget(detail_header_);

    detail_meta_ = new QLabel;
    detail_meta_->setStyleSheet(
        QString("color:%1;font-size:" + fincept::ui::fonts::px(-4) + "px;letter-spacing:0.5px;").arg(ui::colors::TEXT_TERTIARY()));
    status_pill_ = new QLabel;
    status_pill_->hide();
    auto* meta_row = new QHBoxLayout;
    meta_row->setSpacing(8);
    meta_row->addWidget(status_pill_);
    meta_row->addWidget(detail_meta_, 1);
    dv->addLayout(meta_row);

    plan_title_ = new QLabel(tr("PLAN"));
    plan_title_->setStyleSheet(QString("color:%1;font-size:" + fincept::ui::fonts::px(-5) + "px;font-weight:700;letter-spacing:1px;margin-top:6px;")
                                   .arg(ui::colors::TEXT_TERTIARY()));
    dv->addWidget(plan_title_);

    plan_view_ = new QListWidget;
    plan_view_->setSelectionMode(QAbstractItemView::NoSelection);
    plan_view_->setFocusPolicy(Qt::NoFocus);
    plan_view_->setMaximumHeight(180);
    plan_view_->setStyleSheet(
        QString("QListWidget { background:%1; color:%2; border:1px solid %3; "
                "font-size:" + fincept::ui::fonts::px(-3) + "px; }"
                "QListWidget::item { padding:2px 6px; }")
            .arg(ui::colors::BG_SURFACE(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_DIM()));
    dv->addWidget(plan_view_);

    // Budget meter — 4 bars (tokens / cost / wall / steps) in a tight grid.
    budget_label_ = new QLabel(tr("BUDGET"));
    budget_label_->setStyleSheet(QString("color:%1;font-size:" + fincept::ui::fonts::px(-5) + "px;font-weight:700;letter-spacing:1px;margin-top:6px;")
                                     .arg(ui::colors::TEXT_TERTIARY()));
    dv->addWidget(budget_label_);

    auto make_bar = [](const QString& color) {
        auto* b = new QProgressBar;
        b->setMaximumHeight(10);
        b->setTextVisible(false);
        b->setStyleSheet(QString("QProgressBar { background:%1; border:1px solid %2; border-radius:2px; }"
                                 "QProgressBar::chunk { background:%3; }")
                             .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM(), color));
        b->setMinimum(0);
        b->setMaximum(100);
        return b;
    };
    auto* budget_grid = new QGridLayout;
    budget_grid->setVerticalSpacing(2);
    budget_grid->setHorizontalSpacing(8);
    auto add_bar_row = [&](int row, const QString& label, QLabel*& label_out, QProgressBar*& bar,
                           const QString& color) {
        label_out = new QLabel(label);
        label_out->setStyleSheet(QString("color:%1;font-size:" + fincept::ui::fonts::px(-5) + "px;").arg(ui::colors::TEXT_TERTIARY()));
        label_out->setMinimumWidth(60);
        bar = make_bar(color);
        budget_grid->addWidget(label_out, row, 0);
        budget_grid->addWidget(bar, row, 1);
    };
    add_bar_row(0, tr("tokens"), budget_tokens_label_, budget_tokens_bar_, ui::colors::AMBER());
    add_bar_row(1, tr("cost"), budget_cost_label_, budget_cost_bar_, ui::colors::AMBER());
    add_bar_row(2, tr("wall"), budget_wall_label_, budget_wall_bar_, ui::colors::AMBER());
    add_bar_row(3, tr("steps"), budget_steps_label_, budget_steps_bar_, ui::colors::AMBER());
    dv->addLayout(budget_grid);

    // HITL question banner — hidden until task.status == paused_for_input.
    question_banner_ = new QFrame;
    question_banner_->setFrameShape(QFrame::Box);
    question_banner_->setStyleSheet(
        QString("QFrame { background:%1; border:1px solid %2; border-radius:3px; margin-top:6px; }")
            .arg(ui::colors::BG_SURFACE(), ui::colors::AMBER()));
    auto* qbl = new QVBoxLayout(question_banner_);
    qbl->setContentsMargins(8, 8, 8, 8);
    qbl->setSpacing(6);
    question_title_ = new QLabel(tr("AGENT NEEDS INPUT"));
    question_title_->setStyleSheet(
        QString("color:%1;font-size:" + fincept::ui::fonts::px(-5) + "px;font-weight:700;letter-spacing:1px;").arg(ui::colors::AMBER()));
    qbl->addWidget(question_title_);
    question_label_ = new QLabel;
    question_label_->setWordWrap(true);
    question_label_->setStyleSheet(QString("color:%1;font-size:" + fincept::ui::fonts::px(-2) + "px;").arg(ui::colors::TEXT_PRIMARY()));
    qbl->addWidget(question_label_);
    auto* reply_row = new QHBoxLayout;
    reply_edit_ = new QLineEdit;
    reply_edit_->setPlaceholderText(tr("Type your reply…"));
    reply_edit_->setStyleSheet(
        QString("QLineEdit { background:%1; color:%2; border:1px solid %3; padding:4px 8px; font-size:" + fincept::ui::fonts::px(-3) + "px; }")
            .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_DIM()));
    reply_btn_ = new QPushButton(tr("REPLY"));
    reply_btn_->setCursor(Qt::PointingHandCursor);
    reply_btn_->setStyleSheet(QString("QPushButton { background:%1; color:%2; border:1px solid %2; padding:4px 14px; "
                                      "font-size:" + fincept::ui::fonts::px(-4) + "px; font-weight:600; letter-spacing:1px; }"
                                      "QPushButton:hover { background:%2; color:%3; }")
                                  .arg(ui::colors::BG_SURFACE(), ui::colors::AMBER(), ui::colors::TEXT_ON_ACCENT()));
    reply_row->addWidget(reply_edit_, 1);
    reply_row->addWidget(reply_btn_);
    qbl->addLayout(reply_row);
    question_banner_->setVisible(false);
    dv->addWidget(question_banner_);

    // The task's answer, rendered — shown once the task has one.
    final_title_ = new QLabel(tr("RESULT"));
    final_title_->setStyleSheet(QString("color:%1;font-size:" + fincept::ui::fonts::px(-5) + "px;font-weight:700;letter-spacing:1px;margin-top:6px;")
                                    .arg(ui::colors::SUCCESS()));
    final_title_->hide();
    dv->addWidget(final_title_);
    final_view_ = new QTextBrowser;
    final_view_->setOpenExternalLinks(true);
    final_view_->setStyleSheet(QString("QTextBrowser { background:%1; color:%2; border:1px solid %3; padding:6px; }")
                                   .arg(ui::colors::BG_SURFACE(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_DIM()));
    final_view_->hide();
    dv->addWidget(final_view_, 2);

    step_log_title_ = new QLabel(tr("STEP LOG"));
    step_log_title_->setStyleSheet(QString("color:%1;font-size:" + fincept::ui::fonts::px(-5) + "px;font-weight:700;letter-spacing:1px;margin-top:6px;")
                                       .arg(ui::colors::TEXT_TERTIARY()));
    dv->addWidget(step_log_title_);

    step_log_ = new QTextBrowser;
    step_log_->setOpenExternalLinks(true);
    step_log_->setStyleSheet(
        QString("QTextBrowser { background:%1; color:%2; border:1px solid %3; padding:4px; }")
            .arg(ui::colors::BG_SURFACE(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_DIM()));
    dv->addWidget(step_log_, 1);

    // Action buttons
    auto* btn_row = new QHBoxLayout;
    btn_row->setSpacing(6);
    auto make_btn = [](const QString& text, const QString& color) {
        auto* b = new QPushButton(text);
        b->setEnabled(false);
        b->setCursor(Qt::PointingHandCursor);
        b->setStyleSheet(QString("QPushButton { background:%1; color:%2; border:1px solid %2; padding:5px 14px; "
                                 "font-size:" + fincept::ui::fonts::px(-4) + "px; font-weight:600; letter-spacing:1px; }"
                                 "QPushButton:hover { background:%2; color:%4; }"
                                 "QPushButton:disabled { color:%3; border-color:%3; background:%1; }")
                             .arg(ui::colors::BG_SURFACE(), color, ui::colors::TEXT_TERTIARY(),
                                  ui::colors::TEXT_ON_ACCENT()));
        return b;
    };
    pause_btn_ = make_btn(tr("PAUSE"), ui::colors::AMBER());
    resume_btn_ = make_btn(tr("RESUME"), ui::colors::SUCCESS());
    cancel_btn_ = make_btn(tr("CANCEL"), ui::colors::DANGER());
    schedule_btn_ = make_btn(tr("SCHEDULE…"), ui::colors::AMBER());
    schedules_btn_ = make_btn(tr("SCHEDULES…"), ui::colors::TEXT_SECONDARY());
    schedules_btn_->setEnabled(true); // always enabled — lists/removes saved recurring tasks
    schedules_btn_->setToolTip(tr("View, pause and delete recurring scheduled tasks"));
    libraries_btn_ = make_btn(tr("LIBRARIES…"), ui::colors::TEXT_SECONDARY());
    libraries_btn_->setEnabled(true); // always enabled — inspects global stores
    delete_btn_ = make_btn(tr("DELETE"), ui::colors::TEXT_SECONDARY());
    delete_btn_->setProperty("fincept_agent_risk", "delete"); // agents: deletes the task
    resume_btn_->setProperty("fincept_agent_risk", "spend");   // agents: restarts the task's agent (model quota)
    schedule_btn_->setProperty("fincept_agent_risk", "spend"); // agents: arms a recurring task (model quota)
    reply_btn_->setProperty("fincept_agent_risk", "spend");    // agents: sends a reply the agent continues on
    reply_edit_->setProperty("fincept_agent_risk", "spend");   // agents: Enter sends the reply (model quota)
    btn_row->addWidget(pause_btn_);
    btn_row->addWidget(resume_btn_);
    btn_row->addWidget(cancel_btn_);
    btn_row->addWidget(schedule_btn_);
    btn_row->addWidget(schedules_btn_);
    btn_row->addWidget(libraries_btn_);
    btn_row->addStretch();
    btn_row->addWidget(delete_btn_);
    dv->addLayout(btn_row);

    splitter->addWidget(detail);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 2);
    root->addWidget(splitter, 1);

    // Wiring
    connect(refresh_btn_, &QPushButton::clicked, this, &AgenticTasksPanel::refresh_list);
    connect(filter_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &AgenticTasksPanel::on_filter_changed);
    connect(task_list_, &QListWidget::itemClicked, this, &AgenticTasksPanel::on_task_selected);
    connect(pause_btn_, &QPushButton::clicked, this, &AgenticTasksPanel::on_pause_clicked);
    connect(resume_btn_, &QPushButton::clicked, this, &AgenticTasksPanel::on_resume_clicked);
    connect(cancel_btn_, &QPushButton::clicked, this, &AgenticTasksPanel::on_cancel_clicked);
    connect(delete_btn_, &QPushButton::clicked, this, &AgenticTasksPanel::on_delete_clicked);
    connect(reply_btn_, &QPushButton::clicked, this, &AgenticTasksPanel::on_reply_clicked);
    connect(reply_edit_, &QLineEdit::returnPressed, this, &AgenticTasksPanel::on_reply_clicked);
    connect(schedule_btn_, &QPushButton::clicked, this, &AgenticTasksPanel::on_schedule_clicked);
    connect(schedules_btn_, &QPushButton::clicked, this, &AgenticTasksPanel::on_schedules_clicked);
    connect(libraries_btn_, &QPushButton::clicked, this, &AgenticTasksPanel::on_libraries_clicked);
}

void AgenticTasksPanel::wire_service() {
    auto& svc = services::AgentService::instance();
    connect(&svc, &services::AgentService::tasks_listed, this, [this](const QJsonArray& tasks) {
        // Every list_tasks() caller's answer arrives here (the screen's run list asks
        // for the unfiltered list): keep only what this panel's filter shows.
        const QString wanted = filter_combo_->currentData().toString();
        task_list_->clear();
        QListWidgetItem* selected_item = nullptr;
        QString selected_status;
        for (const auto& v : tasks) {
            QJsonObject t = v.toObject();
            const QString id = t.value("id").toString();
            const QString status = t.value("status").toString();
            if (!wanted.isEmpty() && status != wanted)
                continue;
            const QString query = t.value("query").toString();
            int steps = t.value("steps").toArray().size();
            int current = t.value("current_step").toInt();
            const QString label = QString("%1 · %2/%3 · %4")
                                      .arg(status.toUpper(), QString::number(current),
                                           QString::number(qMax(steps, current)), trunc(query, 60));
            auto* item = new QListWidgetItem(label);
            item->setData(kTaskIdRole, id);
            item->setData(kTaskJsonRole, t);
            item->setForeground(QColor(status_color_for(status)));
            task_list_->addItem(item);
            if (!selected_task_id_.isEmpty() && id == selected_task_id_) {
                selected_item = item;
                selected_status = status;
            }
        }
        // Every refresh rebuilt the list and dropped the highlight, so the detail
        // pane showed a task that was no longer selected. Re-select it (no
        // itemClicked fires) and bring the controls in line with its status.
        if (selected_item) {
            task_list_->blockSignals(true);
            task_list_->setCurrentItem(selected_item);
            task_list_->blockSignals(false);
            if (selected_status != shown_status_)
                apply_status_controls(selected_status);
        } else if (!detail_only_ && selected_task_id_.isEmpty() && task_list_->count() > 0) {
            // Never "Select a task to view details" while tasks are listed: open
            // the newest one (a local read of its plan / steps / budget).
            task_list_->setCurrentRow(0);
            on_task_selected(task_list_->item(0));
        }
        if (task_list_->count() > 0) {
            list_stack_->setCurrentWidget(task_list_);
        } else {
            const bool filtered = !filter_combo_->currentData().toString().isEmpty();
            list_empty_->apply(filtered ? ui::EmptyState::no_data_spec(tr("No tasks with this status"),
                                                                       tr("Pick All in the filter above to see every "
                                                                          "task."))
                                        : ui::EmptyState::no_data_spec(
                                              tr("No agentic tasks yet"),
                                              tr("Long-running tasks started from CHAT (Agentic Mode) or a schedule "
                                                 "appear here with their plan, steps and budget."),
                                              tr("SCHEDULE A TASK")));
            list_stack_->setCurrentWidget(list_empty_);
        }
    });
    connect(&svc, &services::AgentService::task_loaded, this, [this](const QJsonObject& task) {
        if (task.value("id").toString() != selected_task_id_)
            return;
        render_task_detail(task);
    });
    connect(&svc, &services::AgentService::task_event, this,
            [this](const QString& task_id, const QJsonObject& event) { apply_task_event(task_id, event); });

    // SCHEDULE… outcome. The step log used to say "[scheduled]" right after
    // Save, even when Python rejected the expression and nothing was created.
    connect(&svc, &services::AgentService::schedule_created, this, [this](const QString&) {
        if (pending_schedule_name_.isEmpty())
            return; // created elsewhere (e.g. an MCP tool)
        log_line(tr("Scheduled '%1' on '%2'").arg(pending_schedule_name_, pending_schedule_expr_), QStringLiteral("done"));
        pending_schedule_name_.clear();
        pending_schedule_expr_.clear();
        pending_schedule_query_.clear();
    });
    connect(&svc, &services::AgentService::error_occurred, this, [this](const QString& ctx, const QString& msg) {
        if (ctx != QLatin1String("schedule_create") || pending_schedule_name_.isEmpty())
            return;
        const QString name = pending_schedule_name_;
        const QString expr = pending_schedule_expr_;
        const QString query = pending_schedule_query_;
        pending_schedule_name_.clear();
        pending_schedule_expr_.clear();
        pending_schedule_query_.clear();
        // Deferred: this runs inside the service's process-finished handler, and
        // the message box / dialog below spin nested event loops.
        QTimer::singleShot(0, this, [this, name, expr, query, msg]() {
            QMessageBox::warning(this, tr("Schedule"), tr("The schedule was not created.\n\n%1").arg(msg));
            open_schedule_dialog(name, expr, query);
        });
    });
}

void AgenticTasksPanel::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    if (first_show_) {
        first_show_ = false;
        stale_ = false;
        refresh_list();
        // AgentService::initialize_deferred() arms saved schedules at startup;
        // listing re-syncs the service's due-time timer with the DB as well
        // (cheap insurance if Agentic Mode was switched on mid-session).
        services::AgentService::instance().schedule_list();
    } else if (stale_) {
        stale_ = false;
        refresh_list();
    }
}

// ── Re-translation ───────────────────────────────────────────────────────────

void AgenticTasksPanel::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void AgenticTasksPanel::retranslateUi() {
    // Filter bar.
    if (filter_title_)
        filter_title_->setText(tr("FILTER:"));
    if (filter_combo_ && filter_combo_->count() >= 8) {
        // Re-apply display text by index; the data role (status code) is preserved.
        filter_combo_->setItemText(0, tr("All"));
        filter_combo_->setItemText(1, tr("Running"));
        filter_combo_->setItemText(2, tr("Paused"));
        filter_combo_->setItemText(3, tr("Completed"));
        filter_combo_->setItemText(4, tr("Failed"));
        filter_combo_->setItemText(5, tr("Cancelled"));
        filter_combo_->setItemText(6, tr("Needs input"));
        filter_combo_->setItemText(7, tr("Budget stop"));
    }
    if (refresh_btn_)
        refresh_btn_->setText(tr("REFRESH"));

    // Detail section titles (detail_header_ / detail_meta_ hold live task data).
    if (plan_title_)
        plan_title_->setText(tr("PLAN"));
    if (step_log_title_)
        step_log_title_->setText(tr("STEP LOG"));

    // Budget meter row labels (budget_label_ holds a live snapshot string).
    if (budget_tokens_label_)
        budget_tokens_label_->setText(tr("tokens"));
    if (budget_cost_label_)
        budget_cost_label_->setText(tr("cost"));
    if (budget_wall_label_)
        budget_wall_label_->setText(tr("wall"));
    if (budget_steps_label_)
        budget_steps_label_->setText(tr("steps"));

    // HITL banner (question_label_ holds the live question text).
    if (question_title_)
        question_title_->setText(tr("AGENT NEEDS INPUT"));
    if (reply_edit_)
        reply_edit_->setPlaceholderText(tr("Type your reply…"));
    if (reply_btn_)
        reply_btn_->setText(tr("REPLY"));

    // Action buttons.
    if (pause_btn_)
        pause_btn_->setText(tr("PAUSE"));
    if (resume_btn_)
        resume_btn_->setText(tr("RESUME"));
    if (cancel_btn_)
        cancel_btn_->setText(tr("CANCEL"));
    if (schedule_btn_)
        schedule_btn_->setText(tr("SCHEDULE…"));
    if (schedules_btn_) {
        schedules_btn_->setText(tr("SCHEDULES…"));
        schedules_btn_->setToolTip(tr("View, pause and delete recurring scheduled tasks"));
    }
    if (libraries_btn_)
        libraries_btn_->setText(tr("LIBRARIES…"));
    if (delete_btn_)
        delete_btn_->setText(tr("DELETE"));
}

void AgenticTasksPanel::refresh_list() {
    if (detail_only_)
        return; // the screen's run list lists the tasks
    const QString status = filter_combo_->currentData().toString();
    services::AgentService::instance().list_tasks(status, 50);
}

void AgenticTasksPanel::set_detail_only(bool on) {
    detail_only_ = on;
    for (QWidget* w : {static_cast<QWidget*>(filter_title_), static_cast<QWidget*>(filter_combo_),
                       static_cast<QWidget*>(refresh_btn_), static_cast<QWidget*>(list_stack_),
                       static_cast<QWidget*>(schedules_btn_), static_cast<QWidget*>(libraries_btn_)})
        w->setVisible(!on);
}

void AgenticTasksPanel::open_task(const QString& task_id, const QJsonObject& row) {
    if (task_id.isEmpty())
        return;
    selected_task_id_ = task_id;
    if (!row.isEmpty())
        render_task_detail(row);
    else
        clear_step_log();
    // The freshest plan / steps / budget (a local read, no model call).
    services::AgentService::instance().get_task(task_id);
}

void AgenticTasksPanel::schedule_new_task() {
    open_schedule_dialog({}, {}, {});
}

void AgenticTasksPanel::request_refresh() {
    if (detail_only_)
        return; // the screen's run list refreshes itself on task events
    if (!isVisible()) {
        stale_ = true; // showEvent catches up
        return;
    }
    if (!refresh_throttle_) {
        refresh_throttle_ = new QTimer(this);
        refresh_throttle_->setSingleShot(true);
        refresh_throttle_->setInterval(1200);
        connect(refresh_throttle_, &QTimer::timeout, this, &AgenticTasksPanel::refresh_list);
    }
    // Leading-edge throttle: the first event arms it, later ones in the window ride along.
    if (!refresh_throttle_->isActive())
        refresh_throttle_->start();
}

void AgenticTasksPanel::on_filter_changed(int) {
    refresh_list();
}

void AgenticTasksPanel::on_task_selected(QListWidgetItem* item) {
    if (!item)
        return;
    selected_task_id_ = item->data(kTaskIdRole).toString();
    QJsonObject task = item->data(kTaskJsonRole).toJsonObject();
    render_task_detail(task);
    // Also re-fetch to get the freshest plan / steps state.
    services::AgentService::instance().get_task(selected_task_id_);
}

void AgenticTasksPanel::render_task_detail(const QJsonObject& task) {
    const QString id = task.value("id").toString();
    const QString status = task.value("status").toString();
    const QString query = task.value("query").toString();
    // The task's own agent policy (tool filter, destructive opt-in, agent id) —
    // handed back on RESUME / REPLY so the resumed run keeps the same rules.
    selected_task_config_ = task.value("config").toObject();
    selected_query_ = query;
    detail_header_->setText(query);
    detail_header_->setStyleSheet(QString("color:%1;font-size:" + fincept::ui::fonts::px(-1) + "px;font-weight:600;").arg(status_color_for(status)));

    int n_steps = task.value("steps").toArray().size();
    int current = task.value("current_step").toInt();
    detail_meta_->setText(tr("step %1 / %2  ·  created %3  ·  id %4")
                              .arg(QString::number(current), QString::number(qMax(n_steps, current)),
                                   agentic_created_text(task.value("created_at").toString()), id.left(8)));
    detail_meta_->setToolTip(id);

    render_plan(task.value("plan").toObject());

    // Question banner: surface persisted pending_question from the row when
    // status==paused_for_input. Live `question` events also fire render_question.
    const QString pending_q = task.value("pending_question").toString();
    if (status == QLatin1String("paused_for_input") && !pending_q.isEmpty())
        render_question(pending_q);
    else
        clear_question();

    // Every persisted step in full (it used to stop at 1,200 characters), rendered.
    step_log_entries_.clear();
    const QJsonArray steps = task.value("steps").toArray();
    for (const auto& sv : steps) {
        QJsonObject s = sv.toObject();
        const QString err = s.value("error").toString();
        const bool failed = !err.isEmpty() || !s.value("success").toBool(true);
        const QString body = failed && s.value("response").toString().isEmpty() ? err : s.value("response").toString();
        step_log_entries_.append({true, failed ? QStringLiteral("error") : QStringLiteral("done"),
                                  tr("Step %1 · %2").arg(s.value("step").toInt()).arg(s.value("description").toString()),
                                  body});
    }
    if (!task.value("error").toString().isEmpty())
        step_log_entries_.append({false, QStringLiteral("error"), task.value("error").toString(), QString()});
    render_step_log();
    // The final answer of a finished task (agent_tasks.result).
    show_final(task.value("result").toString());

    apply_status_controls(status);
    delete_btn_->setEnabled(!id.isEmpty());
    // Schedule action makes sense once a task has actually been run — the
    // user is saying "do THIS workflow again on a cadence." Enabling for
    // every non-empty row keeps the UI minimal.
    schedule_btn_->setEnabled(!id.isEmpty() && !query.isEmpty());
}

void AgenticTasksPanel::apply_status_controls(const QString& status) {
    shown_status_ = status;
    set_status_pill(status);
    detail_header_->setStyleSheet(QString("color:%1;font-size:" + fincept::ui::fonts::px(-1) + "px;font-weight:600;")
                                      .arg(status_color_for(status)));
    const bool active = (status == "running" || status == "pause_requested");
    pause_btn_->setEnabled(active);
    resume_btn_->setEnabled(status == "failed" || status == "paused" || status == "pause_requested" ||
                            status == "cancelled" || status == "failed_budget");
    // A task parked on a clarifying question (paused_for_input) must be cancellable
    // too — otherwise only DELETE could ever stop it.
    cancel_btn_->setEnabled(active || status == "paused" || status == "paused_for_input");
}

void AgenticTasksPanel::render_budget(const QJsonObject& budget) {
    if (budget.isEmpty())
        return;
    auto pct = [](double used, double max_v) {
        if (max_v <= 0)
            return 0;
        return qBound(0, int(used * 100.0 / max_v), 100);
    };
    const int tokens_pct = pct(budget.value("tokens_used").toDouble(), budget.value("max_tokens").toDouble());
    const int cost_pct = pct(budget.value("cost_used").toDouble(), budget.value("max_cost_usd").toDouble());
    const int wall_pct = pct(budget.value("wall_s").toDouble(), budget.value("max_wall_s").toDouble());
    const int steps_pct = pct(budget.value("steps_used").toDouble(), budget.value("max_steps").toDouble());

    budget_tokens_bar_->setValue(tokens_pct);
    budget_cost_bar_->setValue(cost_pct);
    budget_wall_bar_->setValue(wall_pct);
    budget_steps_bar_->setValue(steps_pct);

    budget_label_->setText(QString("BUDGET  ·  tokens %1/%2  ·  $%3/$%4  ·  %5s/%6s  ·  steps %7/%8")
                               .arg(QString::number(budget.value("tokens_used").toInt()),
                                    QString::number(budget.value("max_tokens").toInt()),
                                    QString::number(budget.value("cost_used").toDouble(), 'f', 2),
                                    QString::number(budget.value("max_cost_usd").toDouble(), 'f', 2),
                                    QString::number(budget.value("wall_s").toInt()),
                                    QString::number(budget.value("max_wall_s").toInt()),
                                    QString::number(budget.value("steps_used").toInt()),
                                    QString::number(budget.value("max_steps").toInt())));
}

void AgenticTasksPanel::render_question(const QString& question) {
    question_label_->setText(question);
    reply_edit_->clear();
    reply_edit_->setFocus();
    question_banner_->setVisible(true);
}

void AgenticTasksPanel::clear_question() {
    question_banner_->setVisible(false);
    reply_edit_->clear();
}

void AgenticTasksPanel::render_plan(const QJsonObject& plan) {
    shown_plan_ = plan;
    plan_view_->clear();
    if (plan.isEmpty()) {
        auto* none = new QListWidgetItem(tr("(no plan persisted)"));
        none->setForeground(QColor(ui::colors::TEXT_TERTIARY()));
        plan_view_->addItem(none);
        return;
    }
    const QString name = plan.value("name").toString();
    if (!name.isEmpty()) {
        auto* head = new QListWidgetItem(name);
        QFont f = head->font();
        f.setBold(true);
        head->setFont(f);
        head->setForeground(QColor(ui::colors::TEXT_PRIMARY()));
        plan_view_->addItem(head);
    }
    // One row per step, its status glyph and colour (done / running / failed / pending).
    const QJsonArray steps = plan.value("steps").toArray();
    int i = 1;
    for (const auto& sv : steps) {
        QJsonObject s = sv.toObject();
        const QString status = s.value("status").toString("pending");
        auto* item = new QListWidgetItem(
            QString("%1  %2. %3").arg(agentic_state_glyph(status), QString::number(i++), s.value("name").toString()));
        item->setForeground(QColor(agentic_state_color(status)));
        item->setToolTip(s.value("description").toString());
        plan_view_->addItem(item);
    }
}

void AgenticTasksPanel::log_line(const QString& text, const QString& state) {
    step_log_entries_.append({false, state, text, QString()});
    render_step_log();
}

void AgenticTasksPanel::log_step(int number, const QString& name, const QString& state, const QString& markdown) {
    step_log_entries_.append({true, state, tr("Step %1 · %2").arg(number).arg(name), markdown});
    render_step_log();
}

void AgenticTasksPanel::clear_step_log() {
    step_log_entries_.clear();
    render_step_log();
}

void AgenticTasksPanel::render_step_log() {
    if (!step_log_)
        return;
    // Follow the tail only when the reader is already at the bottom.
    QScrollBar* bar = step_log_->verticalScrollBar();
    const bool at_end = !bar || bar->value() >= bar->maximum() - 4;
    const int keep = bar ? bar->value() : 0;
    QString html;
    for (const auto& e : step_log_entries_) {
        const QString color = agentic_state_color(e.state);
        if (e.step) {
            html += QStringLiteral("<p style=\"margin:8px 0 2px 0;\"><span style=\"color:%1;font-weight:700;\">%2</span>"
                                   "&nbsp;<span style=\"color:%3;font-weight:700;\">%4</span></p>")
                        .arg(color, agentic_state_glyph(e.state), QString(ui::colors::TEXT_PRIMARY()),
                             e.title.toHtmlEscaped());
            if (!e.markdown.trimmed().isEmpty())
                html += ui::MarkdownRenderer::render(e.markdown);
        } else {
            html += QStringLiteral("<p style=\"margin:2px 0;color:%1;\">%2&nbsp;%3</p>")
                        .arg(color, agentic_state_glyph(e.state), e.title.toHtmlEscaped());
        }
    }
    step_log_->setHtml(html);
    if (bar)
        bar->setValue(at_end ? bar->maximum() : qMin(keep, bar->maximum()));
}

void AgenticTasksPanel::show_final(const QString& markdown) {
    const bool has = !markdown.trimmed().isEmpty();
    if (has)
        agent_result::show_markdown(final_view_, markdown);
    else
        agent_result::show_plain(final_view_, QString());
    final_title_->setVisible(has);
    final_view_->setVisible(has);
}

void AgenticTasksPanel::set_status_pill(const QString& status) {
    if (!status_pill_)
        return;
    if (status.isEmpty()) {
        status_pill_->hide();
        return;
    }
    const QString c = status_color_for(status);
    QString label = status;
    label.replace(QLatin1Char('_'), QLatin1Char(' '));
    status_pill_->setText(label.toUpper());
    status_pill_->setStyleSheet(QString("QLabel { color:%1; background:%2; border:1px solid %1; border-radius:3px; "
                                        "padding:1px 8px; font-size:" + fincept::ui::fonts::px(-5) + "px; font-weight:700; "
                                        "letter-spacing:0.5px; }")
                                    .arg(c, ui::colors::alpha(QColor(c), 0.12)));
    status_pill_->show();
}

void AgenticTasksPanel::apply_task_event(const QString& task_id, const QJsonObject& event) {
    // Keep the list in step with the event stream. Re-listing is a Python spawn, so
    // it goes through request_refresh() (rate-limited, deferred while hidden). A
    // task id the list has never seen — started from chat or fired by a schedule —
    // also triggers one; it used to stay invisible until a manual REFRESH.
    request_refresh();
    if (task_id != selected_task_id_)
        return;

    const QString kind = event.value("kind").toString();
    // Every event carries a budget snapshot when available; render it greedily
    // so the meter is always fresh.
    if (event.contains("budget"))
        render_budget(event.value("budget").toObject());

    // PAUSE / RESUME / CANCEL kept the state they had when the row was clicked:
    // a task that paused, finished, failed or asked a question left PAUSE enabled
    // and RESUME disabled. Follow the status the runner just wrote.
    const QString new_status = agentic_status_after_event(kind, event);
    if (!new_status.isEmpty())
        apply_status_controls(new_status);

    if (kind == "plan_ready" || kind == "task_resumed" || kind == "replanned") {
        render_plan(event.value("plan").toObject());
        if (kind == "replanned")
            log_line(tr("Replanned: %1").arg(event.value("reason").toString()), QStringLiteral("warn"));
        if (kind == "task_resumed")
            clear_question();
    } else if (kind == "step_start") {
        const int idx = event.value("step_index").toInt();
        QJsonObject step = event.value("step").toObject();
        log_line(tr("Step %1 · %2 …").arg(idx + 1).arg(step.value("name").toString()), QStringLiteral("running"));
    } else if (kind == "step_end") {
        const int idx = event.value("step_index").toInt();
        QJsonObject r = event.value("result").toObject();
        const QString err = r.value("error").toString();
        const bool failed = !err.isEmpty() || !r.value("success").toBool(true);
        log_step(idx + 1, event.value("step").toObject().value("name").toString(),
                 failed ? QStringLiteral("error") : QStringLiteral("done"),
                 failed && r.value("response").toString().isEmpty() ? err : r.value("response").toString());
    } else if (kind == "reflection") {
        QJsonObject d = event.value("decision").toObject();
        log_line(tr("Reflect: %1 — %2").arg(d.value("decision").toString(), d.value("reason").toString()));
    } else if (kind == "question") {
        render_question(event.value("question").toString());
        log_line(tr("Paused for input: %1").arg(event.value("question").toString()), QStringLiteral("warn"));
    } else if (kind == "paused") {
        log_line(tr("Paused"));
    } else if (kind == "cancelled") {
        log_line(tr("Cancelled"), QStringLiteral("warn"));
    } else if (kind == "budget_stop") {
        log_line(tr("Budget stop: breached %1 — task aborted").arg(event.value("breach").toString()),
                 QStringLiteral("error"));
    } else if (kind == "done") {
        clear_question();
        log_line(tr("Done"), QStringLiteral("done"));
        show_final(event.value("final").toString());
    } else if (kind == "error") {
        log_line(tr("Error: %1").arg(event.value("error").toString()), QStringLiteral("error"));
    }
}

void AgenticTasksPanel::on_reply_clicked() {
    if (selected_task_id_.isEmpty())
        return;
    const QString answer = reply_edit_->text().trimmed();
    if (answer.isEmpty())
        return;
    services::AgentService::instance().reply_to_question(selected_task_id_, answer, selected_task_config_);
    clear_question();
    log_line(tr("You replied: %1").arg(answer));
}

// ── Libraries dialog ────────────────────────────────────────────────────────
// Read-only inspector for the three Phase 3 stores (skills, archival memory,
// reflexion). Built ad-hoc each click — these tables are small (~100s of rows
// at most) so we don't bother caching. Delete buttons are inline; refresh
// happens after each delete.
namespace {
QTableWidget* make_table(QWidget* parent, const QStringList& headers) {
    auto* t = new QTableWidget(0, headers.size(), parent);
    t->setHorizontalHeaderLabels(headers);
    t->verticalHeader()->setVisible(false);
    t->horizontalHeader()->setStretchLastSection(true);
    t->setSelectionBehavior(QAbstractItemView::SelectRows);
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->setStyleSheet(QString("QTableWidget { background:%1; color:%2; border:1px solid %3; font-size:" + fincept::ui::fonts::px(-3) + "px; }"
                             "QHeaderView::section { background:%4; color:%5; padding:4px; border:none; "
                             "border-right:1px solid %3; font-size:" + fincept::ui::fonts::px(-5) + "px; font-weight:700; letter-spacing:1px; }")
                         .arg(ui::colors::BG_SURFACE(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_DIM(),
                              ui::colors::BG_RAISED(), ui::colors::TEXT_SECONDARY()));
    return t;
}

void fill_skills_table(QTableWidget* t, const QJsonArray& rows) {
    t->setRowCount(0);
    for (const auto& v : rows) {
        QJsonObject r = v.toObject();
        int row = t->rowCount();
        t->insertRow(row);
        t->setItem(row, 0, new QTableWidgetItem(r.value("name").toString()));
        t->setItem(row, 1, new QTableWidgetItem(r.value("description").toString()));
        t->setItem(row, 2, new QTableWidgetItem(QString::number(r.value("success_count").toInt())));
        t->setItem(row, 3, new QTableWidgetItem(r.value("last_used_at").toString().left(19)));
        // Store id on column 0 item so delete can find it.
        t->item(row, 0)->setData(Qt::UserRole, r.value("id").toString());
    }
    t->resizeColumnToContents(0);
    t->resizeColumnToContents(2);
}

void fill_memory_table(QTableWidget* t, const QJsonArray& rows) {
    t->setRowCount(0);
    for (const auto& v : rows) {
        QJsonObject r = v.toObject();
        int row = t->rowCount();
        t->insertRow(row);
        t->setItem(row, 0, new QTableWidgetItem(r.value("type").toString()));
        t->setItem(row, 1, new QTableWidgetItem(r.value("user_id").toString()));
        QString content = r.value("content").toString();
        if (content.size() > 160)
            content = content.left(159) + "…";
        t->setItem(row, 2, new QTableWidgetItem(content));
        t->setItem(row, 3, new QTableWidgetItem(r.value("created_at").toString().left(19)));
        t->item(row, 0)->setData(Qt::UserRole, r.value("id").toString());
    }
    t->resizeColumnToContents(0);
    t->resizeColumnToContents(1);
}

void fill_reflexion_table(QTableWidget* t, const QJsonArray& rows) {
    t->setRowCount(0);
    for (const auto& v : rows) {
        QJsonObject r = v.toObject();
        int row = t->rowCount();
        t->insertRow(row);
        t->setItem(row, 0, new QTableWidgetItem(r.value("decision").toString()));
        t->setItem(row, 1, new QTableWidgetItem(QString::number(r.value("step_idx").toInt())));
        QString q = r.value("query").toString();
        if (q.size() > 60)
            q = q.left(59) + "…";
        t->setItem(row, 2, new QTableWidgetItem(q));
        QString reason = r.value("reason").toString();
        if (reason.size() > 100)
            reason = reason.left(99) + "…";
        t->setItem(row, 3, new QTableWidgetItem(reason));
        t->setItem(row, 4, new QTableWidgetItem(r.value("created_at").toString().left(19)));
    }
    t->resizeColumnToContents(0);
    t->resizeColumnToContents(1);
}

/// One list of a library / schedules dialog with its states: a skeleton until the
/// first answer, a designed empty state when there are no rows, an error page with
/// RETRY when the listing failed, else the table.
struct AgentListPage {
    QStackedWidget* stack = nullptr;
    ui::LoadingSkeleton* loading = nullptr;
    ui::EmptyState* state = nullptr;
    QTableWidget* table = nullptr;
    ui::EmptyState::Spec empty; ///< shown for zero rows
    bool error = false;          ///< `state` shows a failure (its primary = RETRY)

    void show_loading() {
        error = false;
        stack->setCurrentWidget(loading);
    }
    void show_rows() {
        error = false;
        if (table->rowCount() > 0) {
            stack->setCurrentWidget(table);
        } else {
            state->apply(empty);
            stack->setCurrentWidget(state);
        }
    }
    void show_error(const QString& message) {
        error = true;
        state->apply(ui::EmptyState::error_spec(
            message.trimmed().isEmpty() ? QCoreApplication::translate("AgenticTasksPanel",
                                                                      "The agent runtime did not answer.")
                                        : message.trimmed(),
            QCoreApplication::translate("AgenticTasksPanel", "Could not load this list"),
            QCoreApplication::translate("AgenticTasksPanel", "RETRY")));
        stack->setCurrentWidget(state);
    }
};

/// Builds the page under `parent` (children under their final parent).
AgentListPage make_list_page(QWidget* parent, const QStringList& headers, const ui::EmptyState::Spec& empty) {
    AgentListPage p;
    p.stack = new QStackedWidget(parent);
    p.loading = new ui::LoadingSkeleton(ui::LoadingSkeleton::Shape::Table, p.stack);
    p.state = new ui::EmptyState(p.stack);
    p.table = make_table(p.stack, headers);
    p.empty = empty;
    p.stack->addWidget(p.loading);
    p.stack->addWidget(p.state);
    p.stack->addWidget(p.table);
    p.stack->setCurrentWidget(p.loading);
    return p;
}
} // namespace

void AgenticTasksPanel::on_libraries_clicked() {
    QDialog dlg(this);
    dlg.setWindowTitle(tr("Agentic libraries"));
    dlg.setMinimumSize(900, 500);

    auto* tabs = new QTabWidget(&dlg);

    // Skills tab
    auto* skills_tab = new QWidget(tabs);
    auto* skills_layout = new QVBoxLayout(skills_tab);
    auto skills = std::make_shared<AgentListPage>(make_list_page(
        skills_tab, {tr("name"), tr("description"), tr("successes"), tr("last_used")},
        ui::EmptyState::no_data_spec(tr("No skills yet"),
                                     tr("Agentic tasks save a reusable skill here when a plan works; later tasks "
                                        "with a similar goal start from it."))));
    auto* skills_table = skills->table;
    auto* skills_del = new QPushButton(tr("Delete selected"), skills_tab);
    skills_del->setProperty("fincept_agent_risk", "delete"); // agents: deletes the selected skills
    skills_layout->addWidget(skills->stack, 1);
    skills_layout->addWidget(skills_del);
    tabs->addTab(skills_tab, tr("Skills"));

    // Memory tab
    auto* mem_tab = new QWidget(tabs);
    auto* mem_layout = new QVBoxLayout(mem_tab);
    auto memory = std::make_shared<AgentListPage>(make_list_page(
        mem_tab, {tr("type"), tr("user_id"), tr("content"), tr("created")},
        ui::EmptyState::no_data_spec(tr("No archival memories"),
                                     tr("Facts and preferences agents decide to keep across runs appear here."))));
    auto* mem_table = memory->table;
    auto* mem_del = new QPushButton(tr("Delete selected"), mem_tab);
    mem_del->setProperty("fincept_agent_risk", "delete"); // agents: deletes the selected memories
    mem_layout->addWidget(memory->stack, 1);
    mem_layout->addWidget(mem_del);
    tabs->addTab(mem_tab, tr("Archival memory"));

    // Reflexion tab — read-only (no useful per-row delete; clear-all isn't worth a UI)
    auto* refl_tab = new QWidget(tabs);
    auto* refl_layout = new QVBoxLayout(refl_tab);
    auto reflexion = std::make_shared<AgentListPage>(make_list_page(
        refl_tab, {tr("decision"), tr("step"), tr("query"), tr("reason"), tr("created")},
        ui::EmptyState::no_data_spec(tr("No reflections yet"),
                                     tr("When an agentic step is judged and retried, the critique that changed "
                                        "the plan is kept here."))));
    refl_layout->addWidget(reflexion->stack, 1);
    tabs->addTab(refl_tab, tr("Reflexion"));

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dlg);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);

    auto* dv = new QVBoxLayout(&dlg);
    dv->setContentsMargins(12, 12, 12, 12);
    dv->addWidget(tabs, 1);
    dv->addWidget(buttons);

    // Wire one-shot data loaders. Disconnect on dialog close to avoid leaks.
    auto& svc = services::AgentService::instance();
    QPointer<QDialog> dlg_ptr = &dlg;
    auto conn1 = QObject::connect(&svc, &services::AgentService::skills_listed, &dlg, [skills](const QJsonArray& arr) {
        fill_skills_table(skills->table, arr);
        skills->show_rows();
    });
    auto conn2 = QObject::connect(&svc, &services::AgentService::archival_listed, &dlg, [memory](const QJsonArray& arr) {
        fill_memory_table(memory->table, arr);
        memory->show_rows();
    });
    auto conn3 =
        QObject::connect(&svc, &services::AgentService::reflexion_listed, &dlg, [reflexion](const QJsonArray& arr) {
            fill_reflexion_table(reflexion->table, arr);
            reflexion->show_rows();
        });
    // A failed listing used to leave a bare header row: say why, offer RETRY.
    QObject::connect(&svc, &services::AgentService::error_occurred, &dlg,
                     [skills, memory, reflexion](const QString& ctx, const QString& msg) {
                         if (ctx == QLatin1String("skills_list"))
                             skills->show_error(msg);
                         else if (ctx == QLatin1String("archival_list"))
                             memory->show_error(msg);
                         else if (ctx == QLatin1String("reflexion_list"))
                             reflexion->show_error(msg);
                     });
    QObject::connect(skills->state, &ui::EmptyState::primary_clicked, &dlg, [skills, &svc]() {
        if (skills->error) {
            skills->show_loading();
            svc.skills_list();
        }
    });
    QObject::connect(memory->state, &ui::EmptyState::primary_clicked, &dlg, [memory, &svc]() {
        if (memory->error) {
            memory->show_loading();
            svc.archival_list();
        }
    });
    QObject::connect(reflexion->state, &ui::EmptyState::primary_clicked, &dlg, [reflexion, &svc]() {
        if (reflexion->error) {
            reflexion->show_loading();
            svc.reflexion_list();
        }
    });

    // Initial loads (local Python reads of the agent task DB; nothing billed).
    svc.skills_list();
    svc.archival_list();
    svc.reflexion_list();

    QObject::connect(skills_del, &QPushButton::clicked, &dlg, [skills_table, &svc]() {
        auto sel = skills_table->selectedItems();
        if (sel.isEmpty())
            return;
        const QString id = skills_table->item(sel.first()->row(), 0)->data(Qt::UserRole).toString();
        if (id.isEmpty())
            return;
        svc.skill_delete(id);
        svc.skills_list();
    });
    QObject::connect(mem_del, &QPushButton::clicked, &dlg, [mem_table, &svc]() {
        auto sel = mem_table->selectedItems();
        if (sel.isEmpty())
            return;
        const QString id = mem_table->item(sel.first()->row(), 0)->data(Qt::UserRole).toString();
        if (id.isEmpty())
            return;
        svc.archival_delete(id);
        svc.archival_list();
    });

    dlg.exec();
    QObject::disconnect(conn1);
    QObject::disconnect(conn2);
    QObject::disconnect(conn3);
}

void AgenticTasksPanel::on_schedule_clicked() {
    // Pull the currently-rendered query from the selected row so the dialog
    // pre-fills with what the user just ran.
    if (selected_task_id_.isEmpty())
        return;
    QString seed_query;
    for (int i = 0; i < task_list_->count(); ++i) {
        auto* it = task_list_->item(i);
        if (it->data(kTaskIdRole).toString() != selected_task_id_)
            continue;
        seed_query = it->data(kTaskJsonRole).toJsonObject().value("query").toString();
        break;
    }
    if (seed_query.isEmpty())
        seed_query = selected_query_; // detail-only: the list is not filled
    open_schedule_dialog({}, {}, seed_query);
}

void AgenticTasksPanel::open_schedule_dialog(const QString& seed_name, const QString& seed_expr,
                                             const QString& seed_query) {
    // Minimal modal — name + DSL expression + query (editable). DSL help is
    // inline so users don't need to read the README.
    QDialog dlg(this);
    dlg.setWindowTitle(tr("Schedule recurring task"));
    dlg.setMinimumWidth(440);

    auto* name_edit = new QLineEdit(&dlg);
    name_edit->setPlaceholderText(tr("e.g. morning portfolio scan"));
    name_edit->setText(seed_name);

    auto* schedule_edit = new QLineEdit(&dlg);
    schedule_edit->setPlaceholderText(tr("every 30m | hourly | daily 09:30 | weekday 16:00"));
    schedule_edit->setText(seed_expr);

    auto* query_edit = new QPlainTextEdit(&dlg);
    query_edit->setPlainText(seed_query);
    query_edit->setMinimumHeight(80);

    auto* help = new QLabel(tr("Forms: <i>every Nm</i>, <i>every Nh</i>, <i>every Nd</i>, "
                               "<i>hourly</i>, <i>daily HH:MM</i>, <i>weekday HH:MM</i> (UTC)"),
                            &dlg);
    help->setWordWrap(true);
    help->setStyleSheet(QString("color:%1;font-size:" + fincept::ui::fonts::px(-4) + "px;").arg(ui::colors::TEXT_TERTIARY()));

    auto* form = new QFormLayout;
    form->addRow(tr("Name"), name_edit);
    form->addRow(tr("Schedule"), schedule_edit);
    form->addRow(help);
    form->addRow(tr("Query"), query_edit);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dlg);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    auto* dv = new QVBoxLayout(&dlg);
    dv->setContentsMargins(16, 16, 16, 16);
    dv->setSpacing(12);
    dv->addLayout(form);
    dv->addWidget(buttons);

    if (dlg.exec() != QDialog::Accepted)
        return;
    const QString name = name_edit->text().trimmed();
    const QString schedule_expr = schedule_edit->text().trimmed();
    const QString query = query_edit->toPlainText().trimmed();
    if (name.isEmpty() || schedule_expr.isEmpty() || query.isEmpty()) {
        QMessageBox::warning(this, tr("Schedule"), tr("Name, schedule, and query are all required."));
        return;
    }
    // Wire through to AgentService; the schedule timer auto-arms. The step log
    // line is written when schedule_created confirms it (see wire_service); a
    // rejected expression re-opens this dialog with the text kept.
    pending_schedule_name_ = name;
    pending_schedule_expr_ = schedule_expr;
    pending_schedule_query_ = query;
    services::AgentService::instance().schedule_create_task(name, query, schedule_expr, {}, false);
}

// ── Schedules dialog ─────────────────────────────────────────────────────────
// Lists the persisted recurring tasks (AgentService::schedule_list) so they can be
// disabled or deleted. SCHEDULE… could create them, but nothing could ever stop a
// recurring (token-spending) task short of editing the SQLite file.
namespace {
void agent_tasks_fill_schedules(QTableWidget* t, const QJsonArray& rows) {
    t->setRowCount(0);
    auto when = [](const QString& iso) { return iso.left(19).replace(QLatin1Char('T'), QLatin1Char(' ')); };
    for (const auto& v : rows) {
        const QJsonObject r = v.toObject();
        const int row = t->rowCount();
        t->insertRow(row);
        const bool enabled = r.value("enabled").toBool();
        auto* name = new QTableWidgetItem(r.value("name").toString());
        name->setData(Qt::UserRole, r.value("id").toString());
        name->setData(Qt::UserRole + 1, enabled);
        t->setItem(row, 0, name);
        t->setItem(row, 1, new QTableWidgetItem(r.value("schedule_expr").toString()));
        t->setItem(row, 2,
                   new QTableWidgetItem(enabled ? QCoreApplication::translate("AgenticTasksPanel", "yes")
                                                : QCoreApplication::translate("AgenticTasksPanel", "no")));
        t->setItem(row, 3, new QTableWidgetItem(when(r.value("next_run_at").toString())));
        t->setItem(row, 4, new QTableWidgetItem(when(r.value("last_run_at").toString())));
        t->setItem(row, 5, new QTableWidgetItem(trunc(r.value("query").toString(), 100)));
    }
    t->resizeColumnToContents(0);
    t->resizeColumnToContents(1);
}
} // namespace

void AgenticTasksPanel::on_schedules_clicked() {
    QDialog dlg(this);
    dlg.setWindowTitle(tr("Scheduled tasks"));
    dlg.setMinimumSize(860, 360);

    auto page = std::make_shared<AgentListPage>(make_list_page(
        &dlg,
        {tr("name"), tr("schedule"), tr("enabled"), tr("next run (UTC)"), tr("last run (UTC)"), tr("query")},
        ui::EmptyState::no_data_spec(tr("No scheduled tasks"),
                                     tr("A schedule runs an agentic task on a cadence (every 30m, daily 09:30, "
                                        "weekday 16:00). Create one here or with SCHEDULE… on a finished task."),
                                     tr("NEW SCHEDULE"))));
    auto* table = page->table;
    auto* toggle_btn = new QPushButton(tr("Enable / disable selected"), &dlg);
    auto* delete_btn = new QPushButton(tr("Delete selected"), &dlg);
    delete_btn->setProperty("fincept_agent_risk", "delete"); // agents: deletes the selected items
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dlg);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    auto* actions = new QHBoxLayout;
    actions->addWidget(toggle_btn);
    actions->addWidget(delete_btn);
    actions->addStretch(1);
    auto* dv = new QVBoxLayout(&dlg);
    dv->setContentsMargins(12, 12, 12, 12);
    dv->addWidget(page->stack, 1);
    dv->addLayout(actions);
    dv->addWidget(buttons);

    auto& svc = services::AgentService::instance();
    // `dlg` is the context object, so the connection dies with the dialog. Toggle and
    // delete both end in a fresh schedules_listed from the service.
    QObject::connect(&svc, &services::AgentService::schedules_listed, &dlg,
                     [page, toggle_btn, delete_btn](const QJsonArray& arr) {
                         agent_tasks_fill_schedules(page->table, arr);
                         page->show_rows();
                         toggle_btn->setEnabled(page->table->rowCount() > 0);
                         delete_btn->setEnabled(page->table->rowCount() > 0);
                     });
    QObject::connect(&svc, &services::AgentService::error_occurred, &dlg, [page](const QString& ctx, const QString& msg) {
        if (ctx == QLatin1String("schedule_list"))
            page->show_error(msg);
    });
    // A schedule created from the empty state's NEW SCHEDULE shows up here at once.
    QObject::connect(&svc, &services::AgentService::schedule_created, &dlg, [&svc]() { svc.schedule_list(); });
    QObject::connect(page->state, &ui::EmptyState::primary_clicked, &dlg, [this, page, &svc]() {
        if (page->error) {
            page->show_loading();
            svc.schedule_list();
            return;
        }
        open_schedule_dialog({}, {}, {}); // NEW SCHEDULE
    });
    toggle_btn->setEnabled(false);
    delete_btn->setEnabled(false);
    svc.schedule_list();

    QObject::connect(toggle_btn, &QPushButton::clicked, &dlg, [table, &svc]() {
        const auto sel = table->selectedItems();
        if (sel.isEmpty())
            return;
        const auto* head = table->item(sel.first()->row(), 0);
        if (!head)
            return;
        svc.schedule_set_enabled(head->data(Qt::UserRole).toString(), !head->data(Qt::UserRole + 1).toBool());
    });
    QObject::connect(delete_btn, &QPushButton::clicked, &dlg, [&dlg, table, &svc]() {
        const auto sel = table->selectedItems();
        if (sel.isEmpty())
            return;
        const auto* head = table->item(sel.first()->row(), 0);
        if (!head)
            return;
        if (QMessageBox::question(&dlg, tr("Delete schedule"),
                                  tr("Delete the schedule \"%1\"?\n\nTasks it already started are not affected.")
                                      .arg(head->text()),
                                  QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
            return;
        svc.schedule_delete(head->data(Qt::UserRole).toString());
    });

    dlg.exec();
}

void AgenticTasksPanel::on_pause_clicked() {
    if (selected_task_id_.isEmpty())
        return;
    services::AgentService::instance().pause_task(selected_task_id_);
}

void AgenticTasksPanel::on_resume_clicked() {
    if (selected_task_id_.isEmpty())
        return;
    services::AgentService::instance().resume_task(selected_task_id_, selected_task_config_);
}

void AgenticTasksPanel::on_cancel_clicked() {
    if (selected_task_id_.isEmpty())
        return;
    services::AgentService::instance().cancel_task(selected_task_id_);
}

void AgenticTasksPanel::on_delete_clicked() {
    if (selected_task_id_.isEmpty())
        return;
    // Cancels a possibly-running subprocess and drops the record permanently —
    // it deserves the same confirmation the schedule dialog already asks for.
    if (QMessageBox::question(this, tr("Delete Task"),
                              tr("Cancel and permanently delete this task?\n\nAny recurring schedule attached to it "
                                 "is removed as well. This cannot be undone."),
                              QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
        return;
    auto& svc = services::AgentService::instance();
    const QString id = selected_task_id_;
    // Cancel first (signals a running subprocess to stop), then actually remove the
    // record. Previously this only called cancel_task, so the row lingered in a
    // "cancelled" state — a scheduled/recurring task kept being re-fired and there was
    // no way to truly delete it (runaway token/credit cost). delete_task removes it.
    svc.cancel_task(id);
    svc.delete_task(id);
    selected_task_id_.clear();
    selected_task_config_ = {};
    selected_query_.clear();
    emit task_deleted(id);
    refresh_list();
    // delete_task is async — refresh once more after it has been applied so the row
    // actually disappears rather than reappearing from the pre-delete snapshot.
    QTimer::singleShot(500, this, [this]() { refresh_list(); });
}

QString AgenticTasksPanel::status_color_for(const QString& status) {
    if (status == "running")
        return ui::colors::AMBER();
    if (status == "completed")
        return ui::colors::SUCCESS();
    if (status == "failed" || status == "error" || status == "failed_budget")
        return ui::colors::DANGER();
    if (status == "paused_for_input")
        return ui::colors::WARNING(); // waiting on the user's reply
    if (status == "paused" || status == "pause_requested")
        return ui::colors::TEXT_SECONDARY();
    if (status == "cancelled" || status == "cancel_requested")
        return ui::colors::TEXT_TERTIARY();
    return ui::colors::TEXT_PRIMARY();
}

} // namespace fincept::screens
