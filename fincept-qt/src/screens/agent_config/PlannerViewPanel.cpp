// src/screens/agent_config/PlannerViewPanel.cpp
#include "screens/agent_config/PlannerViewPanel.h"

#include "core/logging/Logger.h"
#include "services/agents/AgentService.h"
#include "ui/theme/Theme.h"

#include <QApplication>
#include <QClipboard>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSplitter>
#include <QVBoxLayout>

namespace {
static const QString kIn =
    QString("background:%1;color:%2;border:1px solid %3;padding:4px 8px;font-size:12px;")
        .arg(fincept::ui::colors::BG_RAISED, fincept::ui::colors::TEXT_PRIMARY, fincept::ui::colors::BORDER_MED);
} // namespace

namespace fincept::screens {

PlannerViewPanel::PlannerViewPanel(QWidget* parent) : QWidget(parent) {
    setObjectName("PlannerViewPanel");
    build_ui();
    setup_connections();
}

void PlannerViewPanel::build_ui() {
    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    auto* sp = new QSplitter(Qt::Horizontal);
    sp->setHandleWidth(1);
    sp->setStyleSheet(QString("QSplitter::handle{background:%1;}").arg(ui::colors::BORDER_DIM));
    sp->addWidget(build_templates_panel());
    sp->addWidget(build_plan_editor());
    sp->addWidget(build_results_panel());
    sp->setSizes({240, 400, 300});
    sp->setStretchFactor(0, 0);
    sp->setStretchFactor(1, 1);
    sp->setStretchFactor(2, 0);
    root->addWidget(sp);
}

// ── Left ─────────────────────────────────────────────────────────────────────

QWidget* PlannerViewPanel::build_templates_panel() {
    auto* p = new QWidget;
    p->setMinimumWidth(200);
    p->setMaximumWidth(300);
    p->setStyleSheet(
        QString("background:%1;border-right:1px solid %2;").arg(ui::colors::BG_SURFACE, ui::colors::BORDER_DIM));
    auto* vl = new QVBoxLayout(p);
    vl->setContentsMargins(8, 8, 8, 8);
    vl->setSpacing(6);

    auto* t = new QLabel("PLAN TEMPLATES");
    t->setStyleSheet(QString("color:%1;font-size:11px;font-weight:700;letter-spacing:1px;").arg(ui::colors::AMBER));
    vl->addWidget(t);

    template_list_ = new QListWidget;
    template_list_->addItems({"Stock Analysis Plan", "Portfolio Rebalance Plan", "Market Overview Plan",
                              "Risk Assessment Plan", "Sector Rotation Plan"});
    template_list_->setStyleSheet(QString("QListWidget{background:%1;border:1px solid %2;color:%3;font-size:12px;}"
                                          "QListWidget::item{padding:6px 8px;border-bottom:1px solid %2;}"
                                          "QListWidget::item:selected{background:%4;}"
                                          "QListWidget::item:hover{background:%5;}")
                                      .arg(ui::colors::BG_BASE, ui::colors::BORDER_DIM, ui::colors::TEXT_PRIMARY,
                                           ui::colors::AMBER_DIM, ui::colors::BG_HOVER));
    vl->addWidget(template_list_);

    auto* cl = new QLabel("CUSTOM PLAN QUERY");
    cl->setStyleSheet(QString("color:%1;font-size:10px;font-weight:700;letter-spacing:1px;padding-top:8px;")
                          .arg(ui::colors::TEXT_SECONDARY));
    vl->addWidget(cl);

    custom_query_ = new QPlainTextEdit;
    custom_query_->setPlaceholderText("Describe what you want to plan...");
    custom_query_->setMaximumHeight(100);
    custom_query_->setStyleSheet(QString("QPlainTextEdit{%1}").arg(kIn));
    vl->addWidget(custom_query_);

    generate_btn_ = new QPushButton("GENERATE PLAN");
    generate_btn_->setCursor(Qt::PointingHandCursor);
    generate_btn_->setStyleSheet(
        QString("QPushButton{background:%1;color:%2;border:none;padding:8px;"
                "font-size:11px;font-weight:700;letter-spacing:1px;}QPushButton:hover{background:%3;}"
                "QPushButton:disabled{background:%4;color:%5;}")
            .arg(ui::colors::AMBER, ui::colors::BG_BASE, ui::colors::ORANGE, ui::colors::BG_RAISED,
                 ui::colors::TEXT_TERTIARY));
    vl->addWidget(generate_btn_);

    // History section
    history_header_ = new QLabel("PLAN HISTORY");
    history_header_->setStyleSheet(
        QString("color:%1;font-size:10px;font-weight:700;letter-spacing:1px;padding-top:12px;")
            .arg(ui::colors::TEXT_SECONDARY));
    vl->addWidget(history_header_);

    history_search_ = new QLineEdit;
    history_search_->setPlaceholderText("Search history...");
    history_search_->setStyleSheet(kIn);
    vl->addWidget(history_search_);

    history_list_ = new QListWidget;
    history_list_->setMaximumHeight(120);
    history_list_->setStyleSheet(
        QString("QListWidget{background:%1;border:1px solid %2;color:%3;font-size:11px;}"
                "QListWidget::item{padding:4px 6px;border-bottom:1px solid %2;}"
                "QListWidget::item:selected{background:%4;}")
            .arg(ui::colors::BG_BASE, ui::colors::BORDER_DIM, ui::colors::TEXT_PRIMARY, ui::colors::AMBER_DIM));
    vl->addWidget(history_list_);

    vl->addStretch();
    return p;
}

// ── Center ───────────────────────────────────────────────────────────────────

QWidget* PlannerViewPanel::build_plan_editor() {
    auto* p = new QWidget;
    p->setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE));
    auto* vl = new QVBoxLayout(p);
    vl->setContentsMargins(12, 8, 12, 8);
    vl->setSpacing(6);

    // Header
    auto* hdr = new QHBoxLayout;
    auto* t = new QLabel("EXECUTION PLAN");
    t->setStyleSheet(QString("color:%1;font-size:11px;font-weight:700;letter-spacing:1px;").arg(ui::colors::AMBER));
    hdr->addWidget(t);
    plan_status_ = new QLabel;
    plan_status_->setStyleSheet(QString("color:%1;font-size:10px;background:%2;padding:1px 6px;border-radius:2px;")
                                    .arg(ui::colors::TEXT_SECONDARY, ui::colors::BG_RAISED));
    hdr->addWidget(plan_status_);
    hdr->addStretch();
    progress_label_ = new QLabel;
    progress_label_->setStyleSheet(QString("color:%1;font-size:10px;").arg(ui::colors::TEXT_TERTIARY));
    hdr->addWidget(progress_label_);
    vl->addLayout(hdr);

    // Progress bar
    progress_bar_ = new QProgressBar;
    progress_bar_->setFixedHeight(4);
    progress_bar_->setTextVisible(false);
    progress_bar_->setStyleSheet(QString("QProgressBar{background:%1;border:none;}"
                                         "QProgressBar::chunk{background:%2;}")
                                     .arg(ui::colors::BG_RAISED, ui::colors::AMBER));
    progress_bar_->setValue(0);
    vl->addWidget(progress_bar_);

    // Steps table
    steps_table_ = new QTableWidget;
    steps_table_->setColumnCount(4);
    steps_table_->setHorizontalHeaderLabels({"#", "Step", "Type", "Status"});
    steps_table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    steps_table_->setColumnWidth(0, 40);
    steps_table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    steps_table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    steps_table_->horizontalHeader()->setStretchLastSection(true);
    steps_table_->verticalHeader()->setVisible(false);
    steps_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    steps_table_->setEditTriggers(QAbstractItemView::DoubleClicked);
    steps_table_->setStyleSheet(
        QString("QTableWidget{background:%1;color:%2;border:1px solid %3;gridline-color:%3;font-size:12px;}"
                "QTableWidget::item{padding:6px;}"
                "QTableWidget::item:selected{background:%4;}"
                "QHeaderView::section{background:%5;color:%6;border:none;padding:6px;"
                "font-size:10px;font-weight:600;letter-spacing:1px;}")
            .arg(ui::colors::BG_BASE, ui::colors::TEXT_PRIMARY, ui::colors::BORDER_DIM, ui::colors::AMBER_DIM,
                 ui::colors::BG_RAISED, ui::colors::TEXT_SECONDARY));
    vl->addWidget(steps_table_, 1);

    // Step manipulation buttons
    auto* step_btns = new QHBoxLayout;
    auto small_btn = [](const QString& text, const char* color) {
        auto* b = new QPushButton(text);
        b->setCursor(Qt::PointingHandCursor);
        b->setStyleSheet(QString("QPushButton{background:transparent;color:%1;border:1px solid %2;padding:4px 8px;"
                                 "font-size:9px;font-weight:600;}QPushButton:hover{background:%3;}")
                             .arg(color, ui::colors::BORDER_MED, ui::colors::BG_HOVER));
        return b;
    };

    add_step_btn_ = small_btn("+ ADD", ui::colors::POSITIVE);
    remove_step_btn_ = small_btn("- REMOVE", ui::colors::NEGATIVE);
    move_up_btn_ = small_btn("UP", ui::colors::TEXT_SECONDARY);
    move_down_btn_ = small_btn("DOWN", ui::colors::TEXT_SECONDARY);
    step_btns->addWidget(add_step_btn_);
    step_btns->addWidget(remove_step_btn_);
    step_btns->addWidget(move_up_btn_);
    step_btns->addWidget(move_down_btn_);
    step_btns->addStretch();
    vl->addLayout(step_btns);

    // Execute
    execute_btn_ = new QPushButton("EXECUTE PLAN");
    execute_btn_->setCursor(Qt::PointingHandCursor);
    execute_btn_->setEnabled(false);
    execute_btn_->setStyleSheet(
        QString("QPushButton{background:%1;color:%2;border:none;padding:10px;"
                "font-size:11px;font-weight:700;letter-spacing:1px;}QPushButton:hover{background:%1;}"
                "QPushButton:disabled{background:%3;color:%4;}")
            .arg(ui::colors::POSITIVE, ui::colors::TEXT_PRIMARY, ui::colors::BG_RAISED, ui::colors::TEXT_TERTIARY));
    vl->addWidget(execute_btn_);
    return p;
}

// ── Right ────────────────────────────────────────────────────────────────────

QWidget* PlannerViewPanel::build_results_panel() {
    auto* p = new QWidget;
    p->setMinimumWidth(260);
    p->setMaximumWidth(450);
    p->setStyleSheet(
        QString("background:%1;border-left:1px solid %2;").arg(ui::colors::BG_SURFACE, ui::colors::BORDER_DIM));
    auto* vl = new QVBoxLayout(p);
    vl->setContentsMargins(12, 8, 12, 8);
    vl->setSpacing(6);

    auto* hdr = new QHBoxLayout;
    result_header_ = new QLabel("STEP RESULT");
    result_header_->setStyleSheet(
        QString("color:%1;font-size:10px;font-weight:700;letter-spacing:1px;").arg(ui::colors::TEXT_SECONDARY));
    hdr->addWidget(result_header_);
    hdr->addStretch();
    copy_btn_ = new QPushButton("COPY");
    copy_btn_->setCursor(Qt::PointingHandCursor);
    copy_btn_->setStyleSheet(QString("QPushButton{background:transparent;color:%1;border:1px solid %2;padding:2px 8px;"
                                     "font-size:9px;font-weight:600;}QPushButton:hover{background:%3;}")
                                 .arg(ui::colors::TEXT_SECONDARY, ui::colors::BORDER_MED, ui::colors::BG_HOVER));
    hdr->addWidget(copy_btn_);
    vl->addLayout(hdr);

    result_display_ = new QTextEdit;
    result_display_->setReadOnly(true);
    result_display_->setStyleSheet(
        QString("QTextEdit{background:%1;color:%2;border:1px solid %3;padding:8px;font-size:12px;}"
                "QScrollBar:vertical{background:%1;width:6px;}"
                "QScrollBar::handle:vertical{background:%4;min-height:20px;}")
            .arg(ui::colors::BG_BASE, ui::colors::TEXT_PRIMARY, ui::colors::BORDER_DIM, ui::colors::BORDER_BRIGHT));
    vl->addWidget(result_display_, 1);
    return p;
}

// ── Connections ──────────────────────────────────────────────────────────────

void PlannerViewPanel::setup_connections() {
    auto& svc = services::AgentService::instance();

    connect(generate_btn_, &QPushButton::clicked, this, &PlannerViewPanel::generate_plan);
    connect(execute_btn_, &QPushButton::clicked, this, &PlannerViewPanel::execute_plan);
    connect(add_step_btn_, &QPushButton::clicked, this, &PlannerViewPanel::add_step);
    connect(remove_step_btn_, &QPushButton::clicked, this, &PlannerViewPanel::remove_step);
    connect(move_up_btn_, &QPushButton::clicked, this, &PlannerViewPanel::move_step_up);
    connect(move_down_btn_, &QPushButton::clicked, this, &PlannerViewPanel::move_step_down);
    connect(copy_btn_, &QPushButton::clicked, this, &PlannerViewPanel::copy_result);

    connect(&svc, &services::AgentService::plan_created, this, [this](services::ExecutionPlan plan) {
        generating_ = false;
        generate_btn_->setEnabled(true);
        generate_btn_->setText("GENERATE PLAN");
        current_plan_ = plan;
        populate_plan(plan);
        execute_btn_->setEnabled(true);
        save_plan_to_history();
        plan_status_->setText("READY");
        plan_status_->setStyleSheet(QString("color:%1;font-size:10px;background:%2;padding:1px 6px;border-radius:2px;")
                                        .arg(ui::colors::POSITIVE, ui::colors::BG_RAISED));
    });

    connect(&svc, &services::AgentService::plan_executed, this, [this](services::ExecutionPlan plan) {
        executing_ = false;
        execute_btn_->setEnabled(true);
        execute_btn_->setText("EXECUTE PLAN");
        current_plan_ = plan;
        plan_status_->setText(plan.has_failed ? "FAILED" : "COMPLETED");
        plan_status_->setStyleSheet(
            QString("color:%1;font-size:10px;background:%2;padding:1px 6px;border-radius:2px;")
                .arg(plan.has_failed ? ui::colors::NEGATIVE : ui::colors::POSITIVE, ui::colors::BG_RAISED));

        int completed = 0;
        for (int i = 0; i < plan.steps.size() && i < steps_table_->rowCount(); ++i) {
            update_step_status(i, plan.steps[i].status);
            if (plan.steps[i].status == "completed")
                ++completed;
            if (!plan.steps[i].result.isEmpty() || !plan.steps[i].error.isEmpty()) {
                result_display_->setPlainText(plan.steps[i].result.isEmpty() ? plan.steps[i].error
                                                                             : plan.steps[i].result);
                result_header_->setText(QString("STEP %1: %2").arg(i + 1).arg(plan.steps[i].name.toUpper()));
            }
        }
        progress_bar_->setValue(plan.steps.isEmpty() ? 0 : (completed * 100 / plan.steps.size()));
        progress_label_->setText(QString("%1/%2 steps completed").arg(completed).arg(plan.steps.size()));
    });

    connect(&svc, &services::AgentService::error_occurred, this, [this](const QString& ctx, const QString& msg) {
        if (ctx == "create_plan" || ctx == "execute_plan") {
            generating_ = false;
            executing_ = false;
            generate_btn_->setEnabled(true);
            generate_btn_->setText("GENERATE PLAN");
            plan_status_->setText("ERROR");
            plan_status_->setStyleSheet(
                QString("color:%1;font-size:10px;background:%2;padding:1px 6px;border-radius:2px;")
                    .arg(ui::colors::NEGATIVE, ui::colors::BG_RAISED));
            result_display_->setPlainText("Error: " + msg);
        }
    });

    connect(steps_table_, &QTableWidget::currentCellChanged, this, [this](int row, int, int, int) {
        if (row >= 0 && row < current_plan_.steps.size()) {
            const auto& s = current_plan_.steps[row];
            result_header_->setText(QString("STEP %1: %2").arg(row + 1).arg(s.name.toUpper()));
            result_display_->setPlainText(!s.result.isEmpty()  ? s.result
                                          : !s.error.isEmpty() ? "Error: " + s.error
                                                               : "(not yet executed)");
        }
    });

    // History selection
    connect(history_list_, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row >= 0 && row < plan_history_.size()) {
            current_plan_ = plan_history_[row];
            populate_plan(current_plan_);
            execute_btn_->setEnabled(true);
        }
    });

    // History search
    connect(history_search_, &QLineEdit::textChanged, this, [this](const QString& q) {
        for (int i = 0; i < history_list_->count(); ++i)
            history_list_->item(i)->setHidden(!q.isEmpty() &&
                                              !history_list_->item(i)->text().contains(q, Qt::CaseInsensitive));
    });
}

// ── Plan operations ──────────────────────────────────────────────────────────

void PlannerViewPanel::generate_plan() {
    if (generating_)
        return;
    QString q = custom_query_->toPlainText().trimmed();
    if (q.isEmpty()) {
        auto* item = template_list_->currentItem();
        q = item ? item->text() : "Create a comprehensive stock analysis plan";
    }
    generating_ = true;
    generate_btn_->setEnabled(false);
    generate_btn_->setText("GENERATING...");
    steps_table_->setRowCount(0);
    execute_btn_->setEnabled(false);
    result_display_->clear();
    progress_bar_->setValue(0);
    plan_status_->setText("GENERATING");
    plan_status_->setStyleSheet(QString("color:%1;font-size:10px;background:%2;padding:1px 6px;border-radius:2px;")
                                    .arg(ui::colors::AMBER, ui::colors::BG_RAISED));
    services::AgentService::instance().create_plan(q);
}

void PlannerViewPanel::execute_plan() {
    if (executing_ || current_plan_.steps.isEmpty())
        return;
    executing_ = true;
    execute_btn_->setEnabled(false);
    execute_btn_->setText("EXECUTING...");
    result_display_->clear();
    progress_bar_->setValue(0);
    plan_status_->setText("EXECUTING");
    plan_status_->setStyleSheet(QString("color:%1;font-size:10px;background:%2;padding:1px 6px;border-radius:2px;")
                                    .arg(ui::colors::AMBER, ui::colors::BG_RAISED));

    QJsonObject pj;
    pj["id"] = current_plan_.id;
    pj["name"] = current_plan_.name;
    QJsonArray steps;
    for (const auto& s : current_plan_.steps) {
        QJsonObject so;
        so["id"] = s.id;
        so["name"] = s.name;
        so["step_type"] = s.step_type;
        so["config"] = s.config;
        QJsonArray deps;
        for (const auto& d : s.dependencies)
            deps.append(d);
        so["dependencies"] = deps;
        steps.append(so);
    }
    pj["steps"] = steps;
    services::AgentService::instance().execute_plan(pj);
}

void PlannerViewPanel::populate_plan(const services::ExecutionPlan& plan) {
    steps_table_->setRowCount(plan.steps.size());
    for (int i = 0; i < plan.steps.size(); ++i) {
        const auto& s = plan.steps[i];
        auto* num = new QTableWidgetItem(QString::number(i + 1));
        num->setTextAlignment(Qt::AlignCenter);
        num->setFlags(num->flags() & ~Qt::ItemIsEditable);
        steps_table_->setItem(i, 0, num);
        steps_table_->setItem(i, 1, new QTableWidgetItem(s.name));
        steps_table_->setItem(i, 2, new QTableWidgetItem(s.step_type));
        auto* st = new QTableWidgetItem(s.status);
        st->setTextAlignment(Qt::AlignCenter);
        st->setFlags(st->flags() & ~Qt::ItemIsEditable);
        steps_table_->setItem(i, 3, st);
        update_step_status(i, s.status);
    }
    progress_bar_->setValue(0);
    progress_label_->setText(QString("0/%1 steps").arg(plan.steps.size()));
}

void PlannerViewPanel::update_step_status(int row, const QString& status) {
    auto* item = steps_table_->item(row, 3);
    if (!item)
        return;
    item->setText(status.toUpper());
    QColor c;
    if (status == "completed")
        c = QColor(ui::colors::POSITIVE);
    else if (status == "failed")
        c = QColor(ui::colors::NEGATIVE);
    else if (status == "running")
        c = QColor(ui::colors::AMBER);
    else
        c = QColor(ui::colors::TEXT_TERTIARY);
    item->setForeground(c);
}

void PlannerViewPanel::add_step() {
    int row = steps_table_->rowCount();
    services::PlanStep step;
    step.id = QString("step_%1").arg(row + 1);
    step.name = "New Step";
    step.step_type = "run";
    step.status = "pending";
    current_plan_.steps.append(step);
    steps_table_->setRowCount(row + 1);
    auto* num = new QTableWidgetItem(QString::number(row + 1));
    num->setTextAlignment(Qt::AlignCenter);
    num->setFlags(num->flags() & ~Qt::ItemIsEditable);
    steps_table_->setItem(row, 0, num);
    steps_table_->setItem(row, 1, new QTableWidgetItem(step.name));
    steps_table_->setItem(row, 2, new QTableWidgetItem(step.step_type));
    auto* st = new QTableWidgetItem("PENDING");
    st->setTextAlignment(Qt::AlignCenter);
    st->setFlags(st->flags() & ~Qt::ItemIsEditable);
    st->setForeground(QColor(ui::colors::TEXT_TERTIARY));
    steps_table_->setItem(row, 3, st);
}

void PlannerViewPanel::remove_step() {
    int row = steps_table_->currentRow();
    if (row < 0 || row >= current_plan_.steps.size())
        return;
    current_plan_.steps.removeAt(row);
    steps_table_->removeRow(row);
    // Re-number
    for (int i = 0; i < steps_table_->rowCount(); ++i) {
        auto* item = steps_table_->item(i, 0);
        if (item)
            item->setText(QString::number(i + 1));
    }
}

void PlannerViewPanel::move_step_up() {
    int row = steps_table_->currentRow();
    if (row <= 0 || row >= current_plan_.steps.size())
        return;
    current_plan_.steps.swapItemsAt(row, row - 1);
    populate_plan(current_plan_);
    steps_table_->setCurrentCell(row - 1, 0);
}

void PlannerViewPanel::move_step_down() {
    int row = steps_table_->currentRow();
    if (row < 0 || row >= current_plan_.steps.size() - 1)
        return;
    current_plan_.steps.swapItemsAt(row, row + 1);
    populate_plan(current_plan_);
    steps_table_->setCurrentCell(row + 1, 0);
}

void PlannerViewPanel::save_plan_to_history() {
    plan_history_.prepend(current_plan_);
    if (plan_history_.size() > 20)
        plan_history_.removeLast();
    history_list_->clear();
    for (const auto& p : plan_history_)
        history_list_->addItem(QString("%1 (%2 steps)").arg(p.name.isEmpty() ? p.id : p.name).arg(p.steps.size()));
}

void PlannerViewPanel::copy_result() {
    QString text = result_display_->toPlainText();
    if (!text.isEmpty()) {
        QApplication::clipboard()->setText(text);
        copy_btn_->setText("COPIED!");
        QTimer::singleShot(1500, this, [this]() { copy_btn_->setText("COPY"); });
    }
}

} // namespace fincept::screens
