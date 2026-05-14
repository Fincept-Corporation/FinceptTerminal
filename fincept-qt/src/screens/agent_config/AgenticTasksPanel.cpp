// src/screens/agent_config/AgenticTasksPanel.cpp
#include "screens/agent_config/AgenticTasksPanel.h"

#include "core/logging/Logger.h"
#include "services/agents/AgentService.h"
#include "ui/theme/Theme.h"

#include <QComboBox>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QFrame>
#include <QHeaderView>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QShowEvent>
#include <QSplitter>
#include <QVBoxLayout>

namespace fincept::screens {

namespace {
constexpr int kTaskIdRole = Qt::UserRole + 1;
constexpr int kTaskJsonRole = Qt::UserRole + 2;

QString trunc(const QString& s, int n) {
    return s.length() <= n ? s : s.left(n - 1) + QStringLiteral("…");
}
} // namespace

AgenticTasksPanel::AgenticTasksPanel(QWidget* parent) : QWidget(parent) {
    setObjectName("AgenticTasksPanel");
    build_ui();
    wire_service();
}

void AgenticTasksPanel::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(12, 12, 12, 12);
    root->setSpacing(8);

    // ── Filter bar ───────────────────────────────────────────────────────────
    auto* filter_bar = new QHBoxLayout;
    filter_bar->setSpacing(8);

    auto* fl = new QLabel("FILTER:");
    fl->setStyleSheet(QString("color:%1;font-size:10px;letter-spacing:1px;").arg(ui::colors::TEXT_TERTIARY()));
    filter_bar->addWidget(fl);

    filter_combo_ = new QComboBox;
    filter_combo_->addItem("All", QString());
    filter_combo_->addItem("Running", "running");
    filter_combo_->addItem("Paused", "paused");
    filter_combo_->addItem("Completed", "completed");
    filter_combo_->addItem("Failed", "failed");
    filter_combo_->addItem("Cancelled", "cancelled");
    filter_combo_->setStyleSheet(
        QString("QComboBox { background:%1; color:%2; border:1px solid %3; padding:3px 8px; font-size:11px; }")
            .arg(ui::colors::BG_SURFACE(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_DIM()));
    filter_bar->addWidget(filter_combo_);

    refresh_btn_ = new QPushButton("REFRESH");
    refresh_btn_->setCursor(Qt::PointingHandCursor);
    refresh_btn_->setStyleSheet(
        QString("QPushButton { background:%1; color:%2; border:1px solid %3; padding:4px 12px; font-size:10px; "
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

    task_list_ = new QListWidget;
    task_list_->setStyleSheet(
        QString("QListWidget { background:%1; color:%2; border:1px solid %3; font-size:11px; }"
                "QListWidget::item { padding:6px 8px; border-bottom:1px solid %4; }"
                "QListWidget::item:selected { background:%5; color:%6; }")
            .arg(ui::colors::BG_SURFACE(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_DIM(),
                 ui::colors::BORDER_DIM(), ui::colors::BG_RAISED(), ui::colors::AMBER()));
    splitter->addWidget(task_list_);

    // Detail pane
    auto* detail = new QWidget;
    auto* dv = new QVBoxLayout(detail);
    dv->setContentsMargins(8, 0, 0, 0);
    dv->setSpacing(6);

    detail_header_ = new QLabel("Select a task to view details");
    detail_header_->setWordWrap(true);
    detail_header_->setStyleSheet(
        QString("color:%1;font-size:13px;font-weight:600;").arg(ui::colors::AMBER()));
    dv->addWidget(detail_header_);

    detail_meta_ = new QLabel;
    detail_meta_->setStyleSheet(
        QString("color:%1;font-size:10px;letter-spacing:0.5px;").arg(ui::colors::TEXT_TERTIARY()));
    dv->addWidget(detail_meta_);

    auto* plan_label = new QLabel("PLAN");
    plan_label->setStyleSheet(
        QString("color:%1;font-size:9px;font-weight:700;letter-spacing:1px;margin-top:6px;")
            .arg(ui::colors::TEXT_TERTIARY()));
    dv->addWidget(plan_label);

    plan_view_ = new QPlainTextEdit;
    plan_view_->setReadOnly(true);
    plan_view_->setMaximumHeight(180);
    plan_view_->setStyleSheet(
        QString("QPlainTextEdit { background:%1; color:%2; border:1px solid %3; font-family:monospace; "
                "font-size:11px; }")
            .arg(ui::colors::BG_SURFACE(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_DIM()));
    dv->addWidget(plan_view_);

    // Budget meter — 4 bars (tokens / cost / wall / steps) in a tight grid.
    budget_label_ = new QLabel("BUDGET");
    budget_label_->setStyleSheet(
        QString("color:%1;font-size:9px;font-weight:700;letter-spacing:1px;margin-top:6px;")
            .arg(ui::colors::TEXT_TERTIARY()));
    dv->addWidget(budget_label_);

    auto make_bar = [](const QString& color) {
        auto* b = new QProgressBar;
        b->setMaximumHeight(10);
        b->setTextVisible(false);
        b->setStyleSheet(
            QString("QProgressBar { background:%1; border:1px solid %2; border-radius:2px; }"
                    "QProgressBar::chunk { background:%3; }")
                .arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM(), color));
        b->setMinimum(0);
        b->setMaximum(100);
        return b;
    };
    auto* budget_grid = new QGridLayout;
    budget_grid->setVerticalSpacing(2);
    budget_grid->setHorizontalSpacing(8);
    auto add_bar_row = [&](int row, const QString& label, QProgressBar*& bar, const QString& color) {
        auto* lbl = new QLabel(label);
        lbl->setStyleSheet(QString("color:%1;font-size:9px;").arg(ui::colors::TEXT_TERTIARY()));
        lbl->setFixedWidth(60);
        bar = make_bar(color);
        budget_grid->addWidget(lbl, row, 0);
        budget_grid->addWidget(bar, row, 1);
    };
    add_bar_row(0, "tokens", budget_tokens_bar_, ui::colors::AMBER());
    add_bar_row(1, "cost",   budget_cost_bar_,   ui::colors::AMBER());
    add_bar_row(2, "wall",   budget_wall_bar_,   ui::colors::AMBER());
    add_bar_row(3, "steps",  budget_steps_bar_,  ui::colors::AMBER());
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
    auto* qtitle = new QLabel("AGENT NEEDS INPUT");
    qtitle->setStyleSheet(QString("color:%1;font-size:9px;font-weight:700;letter-spacing:1px;")
                              .arg(ui::colors::AMBER()));
    qbl->addWidget(qtitle);
    question_label_ = new QLabel;
    question_label_->setWordWrap(true);
    question_label_->setStyleSheet(QString("color:%1;font-size:12px;").arg(ui::colors::TEXT_PRIMARY()));
    qbl->addWidget(question_label_);
    auto* reply_row = new QHBoxLayout;
    reply_edit_ = new QLineEdit;
    reply_edit_->setPlaceholderText("Type your reply…");
    reply_edit_->setStyleSheet(
        QString("QLineEdit { background:%1; color:%2; border:1px solid %3; padding:4px 8px; font-size:11px; }")
            .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_DIM()));
    reply_btn_ = new QPushButton("REPLY");
    reply_btn_->setCursor(Qt::PointingHandCursor);
    reply_btn_->setStyleSheet(
        QString("QPushButton { background:%1; color:%2; border:1px solid %2; padding:4px 14px; "
                "font-size:10px; font-weight:600; letter-spacing:1px; }"
                "QPushButton:hover { background:%2; color:%1; }")
            .arg(ui::colors::BG_SURFACE(), ui::colors::AMBER()));
    reply_row->addWidget(reply_edit_, 1);
    reply_row->addWidget(reply_btn_);
    qbl->addLayout(reply_row);
    question_banner_->setVisible(false);
    dv->addWidget(question_banner_);

    auto* log_label = new QLabel("STEP LOG");
    log_label->setStyleSheet(
        QString("color:%1;font-size:9px;font-weight:700;letter-spacing:1px;margin-top:6px;")
            .arg(ui::colors::TEXT_TERTIARY()));
    dv->addWidget(log_label);

    step_log_ = new QPlainTextEdit;
    step_log_->setReadOnly(true);
    step_log_->setStyleSheet(
        QString("QPlainTextEdit { background:%1; color:%2; border:1px solid %3; font-family:monospace; "
                "font-size:11px; }")
            .arg(ui::colors::BG_SURFACE(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_DIM()));
    dv->addWidget(step_log_, 1);

    // Action buttons
    auto* btn_row = new QHBoxLayout;
    btn_row->setSpacing(6);
    auto make_btn = [](const QString& text, const QString& color) {
        auto* b = new QPushButton(text);
        b->setEnabled(false);
        b->setCursor(Qt::PointingHandCursor);
        b->setStyleSheet(
            QString("QPushButton { background:%1; color:%2; border:1px solid %2; padding:5px 14px; "
                    "font-size:10px; font-weight:600; letter-spacing:1px; }"
                    "QPushButton:hover { background:%2; color:%1; }"
                    "QPushButton:disabled { color:%3; border-color:%3; background:%1; }")
                .arg(ui::colors::BG_SURFACE(), color, ui::colors::TEXT_TERTIARY()));
        return b;
    };
    pause_btn_ = make_btn("PAUSE", ui::colors::AMBER());
    resume_btn_ = make_btn("RESUME", ui::colors::GREEN());
    cancel_btn_ = make_btn("CANCEL", ui::colors::RED());
    schedule_btn_ = make_btn("SCHEDULE…", ui::colors::AMBER());
    libraries_btn_ = make_btn("LIBRARIES…", ui::colors::TEXT_SECONDARY());
    libraries_btn_->setEnabled(true);  // always enabled — inspects global stores
    delete_btn_ = make_btn("DELETE", ui::colors::TEXT_SECONDARY());
    btn_row->addWidget(pause_btn_);
    btn_row->addWidget(resume_btn_);
    btn_row->addWidget(cancel_btn_);
    btn_row->addWidget(schedule_btn_);
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
    connect(libraries_btn_, &QPushButton::clicked, this, &AgenticTasksPanel::on_libraries_clicked);
}

void AgenticTasksPanel::wire_service() {
    auto& svc = services::AgentService::instance();
    connect(&svc, &services::AgentService::tasks_listed, this, [this](const QJsonArray& tasks) {
        task_list_->clear();
        for (const auto& v : tasks) {
            QJsonObject t = v.toObject();
            const QString id = t.value("id").toString();
            const QString status = t.value("status").toString();
            const QString query = t.value("query").toString();
            int steps = t.value("steps").toArray().size();
            int current = t.value("current_step").toInt();
            const QString label = QString("%1 · %2/%3 · %4")
                                      .arg(status.toUpper(), QString::number(current),
                                           QString::number(qMax(steps, current)),
                                           trunc(query, 60));
            auto* item = new QListWidgetItem(label);
            item->setData(kTaskIdRole, id);
            item->setData(kTaskJsonRole, t);
            item->setForeground(QColor(status_color_for(status)));
            task_list_->addItem(item);
        }
    });
    connect(&svc, &services::AgentService::task_loaded, this, [this](const QJsonObject& task) {
        if (task.value("id").toString() != selected_task_id_) return;
        render_task_detail(task);
    });
    connect(&svc, &services::AgentService::task_event, this,
            [this](const QString& task_id, const QJsonObject& event) {
                apply_task_event(task_id, event);
            });
}

void AgenticTasksPanel::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    if (first_show_) {
        first_show_ = false;
        refresh_list();
    }
}

void AgenticTasksPanel::refresh_list() {
    const QString status = filter_combo_->currentData().toString();
    services::AgentService::instance().list_tasks(status, 50);
}

void AgenticTasksPanel::on_filter_changed(int) {
    refresh_list();
}

void AgenticTasksPanel::on_task_selected(QListWidgetItem* item) {
    if (!item) return;
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
    detail_header_->setText(query);
    detail_header_->setStyleSheet(QString("color:%1;font-size:13px;font-weight:600;")
                                      .arg(status_color_for(status)));

    int n_steps = task.value("steps").toArray().size();
    int current = task.value("current_step").toInt();
    detail_meta_->setText(QString("id: %1  ·  status: %2  ·  step %3 / %4  ·  created: %5")
                              .arg(id.left(8), status.toUpper(), QString::number(current),
                                   QString::number(qMax(n_steps, current)),
                                   task.value("created_at").toString().left(19)));

    render_plan(task.value("plan").toObject());

    // Question banner: surface persisted pending_question from the row when
    // status==paused_for_input. Live `question` events also fire render_question.
    const QString pending_q = task.value("pending_question").toString();
    if (status == QLatin1String("paused_for_input") && !pending_q.isEmpty())
        render_question(pending_q);
    else
        clear_question();

    step_log_->clear();
    const QJsonArray steps = task.value("steps").toArray();
    for (const auto& sv : steps) {
        QJsonObject s = sv.toObject();
        step_log_->appendPlainText(
            QString("[step %1] %2\n%3\n")
                .arg(QString::number(s.value("step").toInt()),
                     s.value("description").toString(),
                     trunc(s.value("response").toString(), 1200)));
    }

    const bool active = (status == "running" || status == "pause_requested");
    pause_btn_->setEnabled(active);
    resume_btn_->setEnabled(status == "failed" || status == "paused" ||
                            status == "pause_requested" || status == "cancelled" ||
                            status == "failed_budget");
    cancel_btn_->setEnabled(active || status == "paused");
    delete_btn_->setEnabled(!id.isEmpty());
    // Schedule action makes sense once a task has actually been run — the
    // user is saying "do THIS workflow again on a cadence." Enabling for
    // every non-empty row keeps the UI minimal.
    schedule_btn_->setEnabled(!id.isEmpty() && !query.isEmpty());
}

void AgenticTasksPanel::render_budget(const QJsonObject& budget) {
    if (budget.isEmpty()) return;
    auto pct = [](double used, double max_v) {
        if (max_v <= 0) return 0;
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

    budget_label_->setText(
        QString("BUDGET  ·  tokens %1/%2  ·  $%3/$%4  ·  %5s/%6s  ·  steps %7/%8")
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
    plan_view_->clear();
    if (plan.isEmpty()) {
        plan_view_->setPlainText("(no plan persisted)");
        return;
    }
    plan_view_->appendPlainText(plan.value("name").toString());
    const QJsonArray steps = plan.value("steps").toArray();
    int i = 1;
    for (const auto& sv : steps) {
        QJsonObject s = sv.toObject();
        const QString status = s.value("status").toString("pending");
        const QString marker = status == "completed" ? "✓" : (status == "running" ? "▶" : "·");
        plan_view_->appendPlainText(QString("  %1 %2. %3")
                                        .arg(marker, QString::number(i++), s.value("name").toString()));
    }
}

void AgenticTasksPanel::apply_task_event(const QString& task_id, const QJsonObject& event) {
    // Update the row in the list regardless of selection.
    for (int i = 0; i < task_list_->count(); ++i) {
        auto* item = task_list_->item(i);
        if (item->data(kTaskIdRole).toString() != task_id) continue;
        // Refresh row by re-listing — cheaper than incremental for Phase 1.
        // (Coalesce avoidance: rely on DataHub policy coalesce_within_ms = 50.)
        refresh_list();
        break;
    }
    if (task_id != selected_task_id_) return;

    const QString kind = event.value("kind").toString();
    // Every event carries a budget snapshot when available; render it greedily
    // so the meter is always fresh.
    if (event.contains("budget")) render_budget(event.value("budget").toObject());

    if (kind == "plan_ready" || kind == "task_resumed" || kind == "replanned") {
        render_plan(event.value("plan").toObject());
        if (kind == "replanned")
            step_log_->appendPlainText(QString("[replanned] %1").arg(event.value("reason").toString()));
        if (kind == "task_resumed") clear_question();
    } else if (kind == "step_start") {
        const int idx = event.value("step_index").toInt();
        QJsonObject step = event.value("step").toObject();
        step_log_->appendPlainText(QString("[step %1] ▶ %2 ...")
                                       .arg(QString::number(idx + 1), step.value("name").toString()));
    } else if (kind == "step_end") {
        const int idx = event.value("step_index").toInt();
        QJsonObject r = event.value("result").toObject();
        step_log_->appendPlainText(QString("[step %1] ✓ %2\n")
                                       .arg(QString::number(idx + 1),
                                            trunc(r.value("response").toString(), 1200)));
    } else if (kind == "reflection") {
        QJsonObject d = event.value("decision").toObject();
        step_log_->appendPlainText(
            QString("[reflect] %1 — %2")
                .arg(d.value("decision").toString(), d.value("reason").toString()));
    } else if (kind == "question") {
        render_question(event.value("question").toString());
        step_log_->appendPlainText(QString("[paused for input] %1").arg(event.value("question").toString()));
    } else if (kind == "paused") {
        step_log_->appendPlainText("[paused]");
    } else if (kind == "cancelled") {
        step_log_->appendPlainText("[cancelled]");
    } else if (kind == "budget_stop") {
        step_log_->appendPlainText(
            QString("[budget_stop] breached %1 — task aborted").arg(event.value("breach").toString()));
    } else if (kind == "done") {
        clear_question();
        step_log_->appendPlainText(QString("[done]\n%1").arg(event.value("final").toString()));
    } else if (kind == "error") {
        step_log_->appendPlainText(QString("[error] %1").arg(event.value("error").toString()));
    }
}

void AgenticTasksPanel::on_reply_clicked() {
    if (selected_task_id_.isEmpty()) return;
    const QString answer = reply_edit_->text().trimmed();
    if (answer.isEmpty()) return;
    services::AgentService::instance().reply_to_question(selected_task_id_, answer);
    clear_question();
    step_log_->appendPlainText(QString("[user reply] %1").arg(answer));
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
    t->setStyleSheet(
        QString("QTableWidget { background:%1; color:%2; border:1px solid %3; font-size:11px; }"
                "QHeaderView::section { background:%4; color:%5; padding:4px; border:none; "
                "border-right:1px solid %3; font-size:9px; font-weight:700; letter-spacing:1px; }")
            .arg(ui::colors::BG_SURFACE(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_DIM(),
                 ui::colors::BG_RAISED(), ui::colors::TEXT_TERTIARY()));
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
        if (content.size() > 160) content = content.left(159) + "…";
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
        if (q.size() > 60) q = q.left(59) + "…";
        t->setItem(row, 2, new QTableWidgetItem(q));
        QString reason = r.value("reason").toString();
        if (reason.size() > 100) reason = reason.left(99) + "…";
        t->setItem(row, 3, new QTableWidgetItem(reason));
        t->setItem(row, 4, new QTableWidgetItem(r.value("created_at").toString().left(19)));
    }
    t->resizeColumnToContents(0);
    t->resizeColumnToContents(1);
}
} // namespace

void AgenticTasksPanel::on_libraries_clicked() {
    QDialog dlg(this);
    dlg.setWindowTitle("Agentic libraries");
    dlg.setMinimumSize(900, 500);

    auto* tabs = new QTabWidget(&dlg);

    // Skills tab
    auto* skills_tab = new QWidget;
    auto* skills_layout = new QVBoxLayout(skills_tab);
    auto* skills_table = make_table(skills_tab,
        {"name", "description", "successes", "last_used"});
    auto* skills_del = new QPushButton("Delete selected");
    skills_layout->addWidget(skills_table, 1);
    skills_layout->addWidget(skills_del);
    tabs->addTab(skills_tab, "Skills");

    // Memory tab
    auto* mem_tab = new QWidget;
    auto* mem_layout = new QVBoxLayout(mem_tab);
    auto* mem_table = make_table(mem_tab,
        {"type", "user_id", "content", "created"});
    auto* mem_del = new QPushButton("Delete selected");
    mem_layout->addWidget(mem_table, 1);
    mem_layout->addWidget(mem_del);
    tabs->addTab(mem_tab, "Archival memory");

    // Reflexion tab — read-only (no useful per-row delete; clear-all isn't worth a UI)
    auto* refl_tab = new QWidget;
    auto* refl_layout = new QVBoxLayout(refl_tab);
    auto* refl_table = make_table(refl_tab,
        {"decision", "step", "query", "reason", "created"});
    refl_layout->addWidget(refl_table, 1);
    tabs->addTab(refl_tab, "Reflexion");

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
    auto conn1 = QObject::connect(&svc, &services::AgentService::skills_listed, &dlg,
        [skills_table](const QJsonArray& arr) { fill_skills_table(skills_table, arr); });
    auto conn2 = QObject::connect(&svc, &services::AgentService::archival_listed, &dlg,
        [mem_table](const QJsonArray& arr) { fill_memory_table(mem_table, arr); });
    auto conn3 = QObject::connect(&svc, &services::AgentService::reflexion_listed, &dlg,
        [refl_table](const QJsonArray& arr) { fill_reflexion_table(refl_table, arr); });

    // Initial loads.
    svc.skills_list();
    svc.archival_list();
    svc.reflexion_list();

    QObject::connect(skills_del, &QPushButton::clicked, &dlg, [skills_table, &svc]() {
        auto sel = skills_table->selectedItems();
        if (sel.isEmpty()) return;
        const QString id = skills_table->item(sel.first()->row(), 0)->data(Qt::UserRole).toString();
        if (id.isEmpty()) return;
        svc.skill_delete(id);
        svc.skills_list();
    });
    QObject::connect(mem_del, &QPushButton::clicked, &dlg, [mem_table, &svc]() {
        auto sel = mem_table->selectedItems();
        if (sel.isEmpty()) return;
        const QString id = mem_table->item(sel.first()->row(), 0)->data(Qt::UserRole).toString();
        if (id.isEmpty()) return;
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
    if (selected_task_id_.isEmpty()) return;
    QString seed_query;
    for (int i = 0; i < task_list_->count(); ++i) {
        auto* it = task_list_->item(i);
        if (it->data(kTaskIdRole).toString() != selected_task_id_) continue;
        seed_query = it->data(kTaskJsonRole).toJsonObject().value("query").toString();
        break;
    }

    // Minimal modal — name + DSL expression + query (editable). DSL help is
    // inline so users don't need to read the README.
    QDialog dlg(this);
    dlg.setWindowTitle("Schedule recurring task");
    dlg.setMinimumWidth(440);

    auto* name_edit = new QLineEdit(&dlg);
    name_edit->setPlaceholderText("e.g. morning portfolio scan");

    auto* schedule_edit = new QLineEdit(&dlg);
    schedule_edit->setPlaceholderText("every 30m | hourly | daily 09:30 | weekday 16:00");

    auto* query_edit = new QPlainTextEdit(&dlg);
    query_edit->setPlainText(seed_query);
    query_edit->setMinimumHeight(80);

    auto* help = new QLabel(
        "Forms: <i>every Nm</i>, <i>every Nh</i>, <i>every Nd</i>, "
        "<i>hourly</i>, <i>daily HH:MM</i>, <i>weekday HH:MM</i> (UTC)", &dlg);
    help->setWordWrap(true);
    help->setStyleSheet(QString("color:%1;font-size:10px;").arg(ui::colors::TEXT_TERTIARY()));

    auto* form = new QFormLayout;
    form->addRow("Name", name_edit);
    form->addRow("Schedule", schedule_edit);
    form->addRow(help);
    form->addRow("Query", query_edit);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dlg);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    auto* dv = new QVBoxLayout(&dlg);
    dv->setContentsMargins(16, 16, 16, 16);
    dv->setSpacing(12);
    dv->addLayout(form);
    dv->addWidget(buttons);

    if (dlg.exec() != QDialog::Accepted) return;
    const QString name = name_edit->text().trimmed();
    const QString schedule_expr = schedule_edit->text().trimmed();
    const QString query = query_edit->toPlainText().trimmed();
    if (name.isEmpty() || schedule_expr.isEmpty() || query.isEmpty()) {
        QMessageBox::warning(this, "Schedule",
                             "Name, schedule, and query are all required.");
        return;
    }
    // Wire through to AgentService; the schedule timer auto-arms.
    services::AgentService::instance().schedule_create_task(name, query, schedule_expr, {}, false);
    step_log_->appendPlainText(
        QString("[scheduled] '%1' on '%2'").arg(name, schedule_expr));
}

void AgenticTasksPanel::on_pause_clicked() {
    if (selected_task_id_.isEmpty()) return;
    services::AgentService::instance().pause_task(selected_task_id_);
}

void AgenticTasksPanel::on_resume_clicked() {
    if (selected_task_id_.isEmpty()) return;
    services::AgentService::instance().resume_task(selected_task_id_);
}

void AgenticTasksPanel::on_cancel_clicked() {
    if (selected_task_id_.isEmpty()) return;
    services::AgentService::instance().cancel_task(selected_task_id_);
}

void AgenticTasksPanel::on_delete_clicked() {
    if (selected_task_id_.isEmpty()) return;
    // Use existing delete_task action via run_python_stdin. There's no
    // dedicated wrapper on AgentService — Phase 2 may add one. For now,
    // request a list refresh; the delete itself happens via the existing
    // PLANNER tab tooling. To keep Phase 1 self-contained we still wire it
    // here by routing through the cancel + list pattern.
    services::AgentService::instance().cancel_task(selected_task_id_);
    selected_task_id_.clear();
    refresh_list();
}

QString AgenticTasksPanel::status_color_for(const QString& status) {
    if (status == "running") return ui::colors::AMBER();
    if (status == "completed") return ui::colors::GREEN();
    if (status == "failed" || status == "error") return ui::colors::RED();
    if (status == "paused" || status == "pause_requested") return ui::colors::TEXT_SECONDARY();
    if (status == "cancelled" || status == "cancel_requested") return ui::colors::TEXT_TERTIARY();
    return ui::colors::TEXT_PRIMARY();
}

} // namespace fincept::screens
