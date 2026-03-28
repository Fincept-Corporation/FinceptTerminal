// src/screens/agent_config/WorkflowsViewPanel.cpp
#include "screens/agent_config/WorkflowsViewPanel.h"

#include "core/logging/Logger.h"
#include "services/agents/AgentService.h"
#include "storage/repositories/LlmProfileRepository.h"
#include "ui/theme/Theme.h"

#include <QComboBox>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QScrollArea>
#include <QSplitter>
#include <QVBoxLayout>

namespace fincept::screens {

// ── Constructor ──────────────────────────────────────────────────────────────

WorkflowsViewPanel::WorkflowsViewPanel(QWidget* parent) : QWidget(parent) {
    setObjectName("WorkflowsViewPanel");
    build_ui();
    setup_connections();
}

// ── UI ───────────────────────────────────────────────────────────────────────

void WorkflowsViewPanel::build_ui() {
    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* splitter = new QSplitter(Qt::Horizontal);
    splitter->setHandleWidth(1);
    splitter->setStyleSheet(QString("QSplitter::handle { background:%1; }").arg(ui::colors::BORDER_DIM));

    splitter->addWidget(build_workflow_cards());
    splitter->addWidget(build_results_panel());
    splitter->setSizes({500, 380});
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 0);

    root->addWidget(splitter);
}

// ── Helper: workflow card ────────────────────────────────────────────────────

QPushButton* WorkflowsViewPanel::make_workflow_card(const QString& title, const QString& desc, const char* color,
                                                    const QString& action_text, QVBoxLayout* parent_layout) {
    auto* card = new QFrame;
    card->setStyleSheet(QString("QFrame { background:%1;border:1px solid %2;padding:12px; }")
                            .arg(ui::colors::BG_SURFACE, ui::colors::BORDER_DIM));

    auto* vl = new QVBoxLayout(card);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(6);

    auto* title_lbl = new QLabel(title);
    title_lbl->setStyleSheet(QString("color:%1;font-size:12px;font-weight:700;letter-spacing:1px;").arg(color));
    vl->addWidget(title_lbl);

    auto* desc_lbl = new QLabel(desc);
    desc_lbl->setWordWrap(true);
    desc_lbl->setStyleSheet(QString("color:%1;font-size:11px;").arg(ui::colors::TEXT_SECONDARY));
    vl->addWidget(desc_lbl);

    auto* btn = new QPushButton(action_text);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setStyleSheet(QString("QPushButton { background:%1;color:%2;border:none;padding:6px;font-size:10px;"
                               "font-weight:700;letter-spacing:1px; }"
                               "QPushButton:hover { opacity:0.8; }"
                               "QPushButton:disabled { background:%3;color:%4; }")
                           .arg(color, ui::colors::BG_BASE, ui::colors::BG_RAISED, ui::colors::TEXT_TERTIARY));
    vl->addWidget(btn);

    parent_layout->addWidget(card);
    return btn;
}

// ── Left: workflow cards ─────────────────────────────────────────────────────

QWidget* WorkflowsViewPanel::build_workflow_cards() {
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setStyleSheet(QString("QScrollArea { background:%1;border:none; }"
                                  "QScrollBar:vertical { background:%1;width:6px; }"
                                  "QScrollBar::handle:vertical { background:%2;min-height:30px; }")
                              .arg(ui::colors::BG_BASE, ui::colors::BORDER_BRIGHT));

    auto* content = new QWidget;
    auto* vl = new QVBoxLayout(content);
    vl->setContentsMargins(16, 12, 16, 12);
    vl->setSpacing(12);

    auto* title = new QLabel("FINANCIAL WORKFLOWS");
    title->setStyleSheet(QString("color:%1;font-size:13px;font-weight:700;letter-spacing:2px;").arg(ui::colors::AMBER));
    vl->addWidget(title);

    // ── LLM Profile picker ───────────────────────────────────────────────────
    auto* profile_row = new QHBoxLayout;
    auto* profile_lbl = new QLabel("LLM PROFILE:");
    profile_lbl->setStyleSheet(QString("color:%1;font-size:10px;font-weight:700;letter-spacing:1px;")
                                   .arg(ui::colors::TEXT_SECONDARY));
    llm_profile_combo_ = new QComboBox;
    llm_profile_combo_->setToolTip("LLM profile used by all workflows on this tab");
    llm_profile_combo_->setStyleSheet(
        QString("QComboBox{background:%1;color:%2;border:1px solid %3;padding:4px 8px;font-size:11px;}"
                "QComboBox::drop-down{border:none;}")
            .arg(ui::colors::BG_RAISED, ui::colors::TEXT_PRIMARY, ui::colors::BORDER_MED));
    // Populate
    llm_profile_combo_->addItem("Default (Global)", QString{});
    const auto pr = fincept::LlmProfileRepository::instance().list_profiles();
    const auto profiles = pr.is_ok() ? pr.value() : QVector<fincept::LlmProfile>{};
    for (const auto& p : profiles)
        llm_profile_combo_->addItem(p.is_default ? p.name + " [default]" : p.name, p.id);

    profile_row->addWidget(profile_lbl);
    profile_row->addWidget(llm_profile_combo_, 1);
    vl->addLayout(profile_row);

    // ── Stock Analysis ───────────────────────────────────────────────────────
    auto* stock_card = new QFrame;
    stock_card->setStyleSheet(QString("QFrame { background:%1;border:1px solid %2;padding:12px; }")
                                  .arg(ui::colors::BG_SURFACE, ui::colors::BORDER_DIM));
    auto* scl = new QVBoxLayout(stock_card);
    scl->setSpacing(6);

    auto* stock_title = new QLabel("STOCK ANALYSIS");
    stock_title->setStyleSheet(
        QString("color:%1;font-size:12px;font-weight:700;letter-spacing:1px;").arg(ui::colors::AMBER));
    scl->addWidget(stock_title);

    auto* stock_desc = new QLabel("Comprehensive stock analysis including fundamentals, technicals, and sentiment.");
    stock_desc->setWordWrap(true);
    stock_desc->setStyleSheet(QString("color:%1;font-size:11px;").arg(ui::colors::TEXT_SECONDARY));
    scl->addWidget(stock_desc);

    auto* sym_row = new QHBoxLayout;
    auto* sym_lbl = new QLabel("Symbol:");
    sym_lbl->setStyleSheet(QString("color:%1;font-size:11px;").arg(ui::colors::TEXT_SECONDARY));
    symbol_input_ = new QLineEdit;
    symbol_input_->setPlaceholderText("AAPL");
    symbol_input_->setMaximumWidth(120);
    symbol_input_->setStyleSheet(
        QString("QLineEdit { background:%1;color:%2;border:1px solid %3;padding:4px 8px;font-size:12px; }")
            .arg(ui::colors::BG_RAISED, ui::colors::TEXT_PRIMARY, ui::colors::BORDER_MED));
    sym_row->addWidget(sym_lbl);
    sym_row->addWidget(symbol_input_);

    auto* analyze_btn = new QPushButton("ANALYZE");
    analyze_btn->setCursor(Qt::PointingHandCursor);
    analyze_btn->setStyleSheet(QString("QPushButton { background:%1;color:%2;border:none;padding:6px 12px;"
                                       "font-size:10px;font-weight:700;letter-spacing:1px; }"
                                       "QPushButton:hover { background:%3; }")
                                   .arg(ui::colors::AMBER, ui::colors::BG_BASE, ui::colors::ORANGE));
    connect(analyze_btn, &QPushButton::clicked, this, [this]() {
        QString sym = symbol_input_->text().trimmed().toUpper();
        if (!sym.isEmpty())
            run_workflow("stock_analysis", {{"symbol", sym}});
    });
    sym_row->addWidget(analyze_btn);
    sym_row->addStretch();
    scl->addLayout(sym_row);
    vl->addWidget(stock_card);

    // ── Other workflow cards using grid ───────────────────────────────────────
    auto* grid = new QGridLayout;
    grid->setSpacing(8);

    // Use a temporary QVBoxLayout wrapper to collect cards via make_workflow_card
    auto* col00 = new QVBoxLayout;
    col00->setContentsMargins(0, 0, 0, 0);
    auto* col01 = new QVBoxLayout;
    col01->setContentsMargins(0, 0, 0, 0);
    auto* col10 = new QVBoxLayout;
    col10->setContentsMargins(0, 0, 0, 0);
    auto* col11 = new QVBoxLayout;
    col11->setContentsMargins(0, 0, 0, 0);

    // Portfolio Rebalancing
    auto* rebal_btn =
        make_workflow_card("PORTFOLIO REBALANCING", "Optimize portfolio allocation and suggest rebalancing trades.",
                           ui::colors::CYAN, "REBALANCE", col00);
    connect(rebal_btn, &QPushButton::clicked, this, [this]() { run_workflow("portfolio_rebal"); });
    grid->addLayout(col00, 0, 0);

    // Risk Assessment
    auto* risk_btn = make_workflow_card("RISK ASSESSMENT", "Analyze portfolio risk metrics, VaR, and stress testing.",
                                        ui::colors::NEGATIVE, "ASSESS RISK", col01);
    connect(risk_btn, &QPushButton::clicked, this, [this]() { run_workflow("risk_assessment"); });
    grid->addLayout(col01, 0, 1);

    // Multi-Query
    auto* mq_btn = make_workflow_card("MULTI-QUERY", "Execute a query across multiple agents simultaneously.",
                                      ui::colors::INFO, "EXECUTE", col10);
    connect(mq_btn, &QPushButton::clicked, this, [this]() { run_workflow("execute_multi_query"); });
    grid->addLayout(col10, 1, 0);

    // Session & Memory
    auto* sess_btn = make_workflow_card("SESSION & MEMORY", "Save agent sessions and manage memory storage.",
                                        ui::colors::WARNING, "SAVE SESSION", col11);
    connect(sess_btn, &QPushButton::clicked, this, [this]() { run_workflow("save_session"); });
    grid->addLayout(col11, 1, 1);

    vl->addLayout(grid);
    vl->addStretch();

    scroll->setWidget(content);
    return scroll;
}

// ── Right: results ───────────────────────────────────────────────────────────

QWidget* WorkflowsViewPanel::build_results_panel() {
    auto* panel = new QWidget;
    panel->setMinimumWidth(320);
    panel->setMaximumWidth(500);
    panel->setStyleSheet(
        QString("background:%1;border-left:1px solid %2;").arg(ui::colors::BG_SURFACE, ui::colors::BORDER_DIM));

    auto* vl = new QVBoxLayout(panel);
    vl->setContentsMargins(12, 8, 12, 8);
    vl->setSpacing(6);

    workflow_title_ = new QLabel("WORKFLOW RESULT");
    workflow_title_->setStyleSheet(
        QString("color:%1;font-size:10px;font-weight:700;letter-spacing:1px;").arg(ui::colors::AMBER));
    vl->addWidget(workflow_title_);

    result_status_ = new QLabel;
    result_status_->setStyleSheet(QString("color:%1;font-size:10px;padding:2px 0;").arg(ui::colors::TEXT_TERTIARY));
    vl->addWidget(result_status_);

    result_display_ = new QTextEdit;
    result_display_->setReadOnly(true);
    result_display_->setStyleSheet(
        QString("QTextEdit { background:%1;color:%2;border:1px solid %3;padding:8px;font-size:12px; }"
                "QScrollBar:vertical { background:%1;width:6px; }"
                "QScrollBar::handle:vertical { background:%4;min-height:20px; }")
            .arg(ui::colors::BG_BASE, ui::colors::TEXT_PRIMARY, ui::colors::BORDER_DIM, ui::colors::BORDER_BRIGHT));
    vl->addWidget(result_display_, 1);

    return panel;
}

// ── Connections ──────────────────────────────────────────────────────────────

void WorkflowsViewPanel::setup_connections() {
    auto& svc = services::AgentService::instance();

    connect(&svc, &services::AgentService::agent_result, this, [this](services::AgentExecutionResult r) {
        if (r.request_id != pending_request_id_) return;
        executing_ = false;
        pending_request_id_.clear();
        if (r.success) {
            result_display_->setPlainText(r.response);
            result_status_->setText(QString("Completed in %1ms").arg(r.execution_time_ms));
            result_status_->setStyleSheet(QString("color:%1;font-size:10px;padding:2px 0;").arg(ui::colors::POSITIVE));
        } else {
            result_display_->setPlainText("Error: " + r.error);
            result_status_->setText("FAILED");
            result_status_->setStyleSheet(QString("color:%1;font-size:10px;padding:2px 0;").arg(ui::colors::NEGATIVE));
        }
    });

    connect(&svc, &services::AgentService::error_occurred, this,
            [this](const QString&, const QString& msg) {
                if (!executing_) return;
                executing_ = false;
                pending_request_id_.clear();
                result_display_->setPlainText("Error: " + msg);
                result_status_->setText("ERROR");
                result_status_->setStyleSheet(
                    QString("color:%1;font-size:10px;padding:2px 0;").arg(ui::colors::NEGATIVE));
            });
}

// ── Run workflow ─────────────────────────────────────────────────────────────

void WorkflowsViewPanel::run_workflow(const QString& type, const QJsonObject& params) {
    if (executing_)
        return;

    executing_ = true;
    result_display_->clear();
    workflow_title_->setText(QString("WORKFLOW: %1").arg(type.toUpper()));
    result_status_->setText("Executing...");
    result_status_->setStyleSheet(QString("color:%1;font-size:10px;padding:2px 0;").arg(ui::colors::AMBER));

    QJsonObject p = params;
    const QString profile_id = llm_profile_combo_ ? llm_profile_combo_->currentData().toString() : QString{};
    if (!profile_id.isEmpty())
        p["llm_profile_id"] = profile_id;

    pending_request_id_ = services::AgentService::instance().run_workflow(type, p);
}

void WorkflowsViewPanel::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    if (!data_loaded_) {
        data_loaded_ = true;
    }
}

} // namespace fincept::screens
