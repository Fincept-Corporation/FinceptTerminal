// src/screens/ai_quant_lab/QuantModulePanel_DeepAgent.cpp
//
// Deep Agent panel — tabbed: LangGraph Deep Analysis + RD-Agent Research.
// Extracted from QuantModulePanel.cpp to keep that file maintainable.
#include "screens/ai_quant_lab/QuantModulePanel.h"
#include "screens/ai_quant_lab/QuantModulePanel_Common.h"
#include "screens/ai_quant_lab/QuantModulePanel_GsHelpers.h"
#include "screens/ai_quant_lab/QuantModulePanel_Styles.h"

#include "services/ai_quant_lab/AIQuantLabService.h"
#include "ui/theme/Theme.h"

#include <QAbstractItemView>
#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QString>
#include <QTabWidget>
#include <QTableWidget>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QWidget>

namespace fincept::screens {

using namespace fincept::services::quant;
using namespace fincept::screens::quant_styles;
using namespace fincept::screens::quant_common;
using namespace fincept::screens::quant_gs_helpers;

// ═══════════════════════════════════════════════════════════════════════════════
// ═══════════════════════════════════════════════════════════════════════════════
// DEEP AGENT PANEL — tabbed: LangGraph Deep Analysis + RD-Agent Research
// ═══════════════════════════════════════════════════════════════════════════════

QWidget* QuantModulePanel::build_deep_agent_panel() {
    auto* w = new QWidget(this);
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    // ── Shared LLM profile picker (used by both Deep Analysis and RD-Agent) ──
    QComboBox* llm_combo = nullptr;
    auto* llm_bar = new QWidget(w);
    llm_bar->setStyleSheet(
        QString("background:%1; border-bottom:1px solid %2;").arg(ui::colors::BG_RAISED(), ui::colors::BORDER_DIM()));
    auto* llm_bar_l = new QHBoxLayout(llm_bar);
    llm_bar_l->setContentsMargins(16, 8, 16, 8);
    llm_bar_l->setSpacing(0);
    llm_bar_l->addWidget(build_llm_picker(llm_bar, &llm_combo));
    vl->addWidget(llm_bar);

    auto* tabs = new QTabWidget(w);
    tabs->setStyleSheet(tab_ss(module_.color.name()));

    // ── Tab 1: LangGraph Deep Analysis ──────────────────────────────────────
    auto* da_w = new QWidget(tabs);
    auto* da_vl = new QVBoxLayout(da_w);
    da_vl->setContentsMargins(16, 16, 16, 16);
    da_vl->setSpacing(12);

    auto* desc =
        new QLabel("Multi-agent financial analysis powered by LangGraph. Delegates to specialist subagents "
                   "(research, data-analyst, trading, risk-analyzer, portfolio-optimizer, backtester, reporter).",
                   da_w);
    desc->setWordWrap(true);
    desc->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3;")
                            .arg(ui::colors::TEXT_SECONDARY())
                            .arg(ui::fonts::SMALL)
                            .arg(ui::fonts::DATA_FAMILY));
    da_vl->addWidget(desc);

    auto* task_edit = new QTextEdit(da_w);
    task_edit->setPlaceholderText("Describe your analysis task...\n"
                                  "e.g. \"Conduct a full investment analysis of NVDA: research fundamentals, "
                                  "assess risks, and give a buy/sell/hold recommendation with price target\"");
    task_edit->setFixedHeight(90);
    task_edit->setStyleSheet(QString("QTextEdit { background:%1; color:%2; border:1px solid %3;"
                                     "font-family:%4; font-size:%5px; padding:6px; }")
                                 .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_MED())
                                 .arg(ui::fonts::DATA_FAMILY)
                                 .arg(ui::fonts::SMALL));
    da_vl->addWidget(build_input_row("Task", task_edit, da_w));

    auto* agent_type = new QComboBox(da_w);
    agent_type->addItems({"general", "research", "trading_strategy", "portfolio_management", "risk_assessment"});
    agent_type->setStyleSheet(combo_ss());
    combo_inputs_["agent_type"] = agent_type;
    da_vl->addWidget(build_input_row("Agent Type", agent_type, da_w));

    auto* thread_id = new QLineEdit(da_w);
    thread_id->setPlaceholderText("Optional — leave blank to auto-generate");
    thread_id->setStyleSheet(QString("QLineEdit { background:%1; color:%2; border:1px solid %3;"
                                     "font-family:%4; font-size:%5px; padding:6px 8px; }")
                                 .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_MED())
                                 .arg(ui::fonts::DATA_FAMILY)
                                 .arg(ui::fonts::SMALL));
    text_inputs_["thread_id"] = thread_id;
    da_vl->addWidget(build_input_row("Thread ID", thread_id, da_w));

    auto* da_run = make_run_button("RUN DEEP ANALYSIS", da_w);
    connect(da_run, &QPushButton::clicked, this, [this, task_edit, agent_type, thread_id, llm_combo]() {
        auto task = task_edit->toPlainText().trimmed();
        if (task.isEmpty()) {
            display_error("Please enter an analysis task.");
            return;
        }
        auto cfg = llm_config_from_combo(llm_combo);
        if (cfg.isEmpty()) {
            display_error("Select an LLM profile before running.");
            return;
        }
        status_label_->setText("Running...");
        clear_results();
        QJsonObject params;
        params["task"] = task;
        params["agent_type"] = agent_type->currentText();
        params["config"] = cfg;
        auto tid = thread_id->text().trimmed();
        if (!tid.isEmpty())
            params["thread_id"] = tid;
        AIQuantLabService::instance().run_deep_agent(params);
    });
    da_vl->addWidget(da_run);

    agent_output_ = new QTextEdit(da_w);
    agent_output_->setReadOnly(true);
    agent_output_->setPlaceholderText("Analysis results will appear here...");
    agent_output_->setMinimumHeight(300);
    agent_output_->setStyleSheet(output_ss());
    da_vl->addWidget(agent_output_, 1);

    auto* rc = new QWidget(da_w);
    results_layout_ = new QVBoxLayout(rc);
    results_layout_->setContentsMargins(0, 0, 0, 0);
    results_layout_->setSpacing(0);
    da_vl->addWidget(rc);

    tabs->addTab(da_w, "Deep Analysis");

    // ── Tab 2: RD-Agent ─────────────────────────────────────────────────────
    tabs->addTab(build_rd_agent_tab(llm_combo), "RD-Agent");

    vl->addWidget(tabs, 1);
    return w;
}

// ═══════════════════════════════════════════════════════════════════════════════
// RD-AGENT TAB — Autonomous factor/model research (Microsoft RD-Agent)
// ═══════════════════════════════════════════════════════════════════════════════

QWidget* QuantModulePanel::build_rd_agent_tab(QComboBox* llm_combo) {
    auto* w = new QWidget(this);
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    // shared styles

    auto* sub = new QTabWidget(w);
    sub->setStyleSheet(sub_tab_ss(module_.color.name()));

    // ── Status bar ───────────────────────────────────────────────────────────
    auto* status_bar = new QWidget(w);
    status_bar->setFixedHeight(28);
    status_bar->setStyleSheet(
        QString("background:%1; border-bottom:1px solid %2;").arg(ui::colors::BG_RAISED(), ui::colors::BORDER_DIM()));
    auto* sbl = new QHBoxLayout(status_bar);
    sbl->setContentsMargins(12, 0, 12, 0);
    sbl->setSpacing(8);
    auto* status_dot = new QLabel("●", status_bar);
    status_dot->setStyleSheet(QString("color:%1;").arg(ui::colors::TEXT_TERTIARY()));
    sbl->addWidget(status_dot);
    auto* status_txt = new QLabel("RD-Agent ready", status_bar);
    status_txt->setObjectName("rdStatusTxt");
    status_txt->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3;")
                                  .arg(ui::colors::TEXT_TERTIARY())
                                  .arg(ui::fonts::TINY)
                                  .arg(ui::fonts::DATA_FAMILY));
    sbl->addWidget(status_txt, 1);
    auto* check_btn = new QPushButton("CHECK STATUS", status_bar);
    check_btn->setCursor(Qt::PointingHandCursor);
    check_btn->setStyleSheet(QString("QPushButton { background:transparent; color:%1; border:1px solid %1;"
                                     "font-family:%2; font-size:%3px; padding:2px 8px; border-radius:2px; }"
                                     "QPushButton:hover { background:%1; color:%4; }")
                                 .arg(module_.color.name())
                                 .arg(ui::fonts::DATA_FAMILY)
                                 .arg(ui::fonts::TINY)
                                 .arg(ui::colors::BG_BASE()));
    connect(check_btn, &QPushButton::clicked, this, [status_txt]() {
        status_txt->setText("Checking...");
        AIQuantLabService::instance().rd_agent_check_status();
    });
    sbl->addWidget(check_btn);

    auto* ui_btn = new QPushButton("OPEN LOG VIEWER", status_bar);
    ui_btn->setCursor(Qt::PointingHandCursor);
    ui_btn->setStyleSheet(QString("QPushButton { background:transparent; color:%1; border:1px solid %1;"
                                  "font-family:%2; font-size:%3px; padding:2px 8px; border-radius:2px; }"
                                  "QPushButton:hover { background:%1; color:%4; }")
                              .arg(ui::colors::TEXT_TERTIARY())
                              .arg(ui::fonts::DATA_FAMILY)
                              .arg(ui::fonts::TINY)
                              .arg(ui::colors::BG_BASE()));
    connect(ui_btn, &QPushButton::clicked, this, [status_txt]() {
        status_txt->setText("Starting log viewer...");
        AIQuantLabService::instance().rd_agent_start_ui();
    });
    sbl->addWidget(ui_btn);

    auto* mcp_btn = new QPushButton("MCP TOOLS", status_bar);
    mcp_btn->setCursor(Qt::PointingHandCursor);
    mcp_btn->setCheckable(true);
    mcp_btn->setToolTip("Start/stop the Fincept MCP tool server\n"
                        "Gives RD-Agent loops access to market data,\n"
                        "financial news and economics tools.");
    mcp_btn->setStyleSheet(QString("QPushButton { background:transparent; color:%1; border:1px solid %1;"
                                   "font-family:%2; font-size:%3px; padding:2px 8px; border-radius:2px; }"
                                   "QPushButton:checked { background:%4; color:%5; border-color:%4; }"
                                   "QPushButton:hover { background:%1; color:%5; }")
                               .arg(ui::colors::TEXT_TERTIARY())
                               .arg(ui::fonts::DATA_FAMILY)
                               .arg(ui::fonts::TINY)
                               .arg(module_.color.name())
                               .arg(ui::colors::BG_BASE()));
    connect(mcp_btn, &QPushButton::toggled, this, [mcp_btn, status_txt](bool checked) {
        if (checked) {
            status_txt->setText("Starting MCP tool server...");
            AIQuantLabService::instance().rd_agent_start_mcp_server();
        } else {
            status_txt->setText("MCP tool server stopped");
            mcp_btn->setText("MCP TOOLS");
        }
    });
    sbl->addWidget(mcp_btn);
    vl->addWidget(status_bar);

    // ── Sub-tab 1: Factor Mining ─────────────────────────────────────────────
    auto* fm_w = new QWidget(sub);
    auto* fm_vl = new QVBoxLayout(fm_w);
    fm_vl->setContentsMargins(14, 14, 14, 14);
    fm_vl->setSpacing(10);

    auto* fm_desc = new QLabel("Autonomous alpha factor discovery via FactorRDLoop. The agent proposes, codes, runs "
                               "and evaluates factors iteratively until the target IC is reached.",
                               fm_w);
    fm_desc->setWordWrap(true);
    fm_desc->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3;")
                               .arg(ui::colors::TEXT_SECONDARY())
                               .arg(ui::fonts::SMALL)
                               .arg(ui::fonts::DATA_FAMILY));
    fm_vl->addWidget(fm_desc);

    auto* fm_task = new QTextEdit(fm_w);
    fm_task->setPlaceholderText(
        "Describe the factor hypothesis...\ne.g. \"Discover momentum-based alpha factors for US equities\"");
    fm_task->setFixedHeight(70);
    fm_task->setStyleSheet(input_ss());
    fm_vl->addWidget(build_input_row("Task Description", fm_task, fm_w));

    auto* fm_market = new QComboBox(fm_w);
    fm_market->addItems({"US", "CN", "EU", "IN", "JP", "HK", "GLOBAL"});
    fm_market->setStyleSheet(combo_ss());
    fm_vl->addWidget(build_input_row("Target Market", fm_market, fm_w));

    auto* fm_iter = new QSpinBox(fm_w);
    fm_iter->setRange(1, 100);
    fm_iter->setValue(10);
    fm_iter->setStyleSheet(input_ss());
    fm_vl->addWidget(build_input_row("Max Iterations", fm_iter, fm_w));

    auto* fm_ic = make_double_spin(0.01, 0.50, 0.05, 3, "", fm_w);
    fm_vl->addWidget(build_input_row("Target IC", fm_ic, fm_w));

    auto* fm_run = make_run_button("START FACTOR MINING", fm_w);
    connect(fm_run, &QPushButton::clicked, this, [this, fm_task, fm_market, fm_iter, fm_ic, status_txt, llm_combo]() {
        auto desc_text = fm_task->toPlainText().trimmed();
        if (desc_text.isEmpty()) {
            display_error("Enter a task description.");
            return;
        }
        auto cfg = llm_config_from_combo(llm_combo);
        if (cfg.isEmpty()) {
            display_error("Select an LLM profile before running.");
            return;
        }
        status_label_->setText("Starting...");
        status_txt->setText("Factor mining started...");
        QJsonObject params;
        params["task_description"] = desc_text;
        params["target_market"] = fm_market->currentText();
        params["max_iterations"] = fm_iter->value();
        params["target_ic"] = fm_ic->value();
        params["config"] = cfg;
        AIQuantLabService::instance().rd_agent_start_factor_mining(params);
    });
    fm_vl->addWidget(fm_run);
    fm_vl->addStretch();
    sub->addTab(fm_w, "Factor Mining");

    // ── Sub-tab 2: Model Optimization ────────────────────────────────────────
    auto* mo_w = new QWidget(sub);
    auto* mo_vl = new QVBoxLayout(mo_w);
    mo_vl->setContentsMargins(14, 14, 14, 14);
    mo_vl->setSpacing(10);

    auto* mo_desc = new QLabel("ML model hyperparameter optimization via ModelRDLoop. Supports LightGBM, XGBoost, "
                               "LSTM, GRU, Transformer and TCN. Optimizes for Sharpe, IC, max drawdown, or win rate.",
                               mo_w);
    mo_desc->setWordWrap(true);
    mo_desc->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3;")
                               .arg(ui::colors::TEXT_SECONDARY())
                               .arg(ui::fonts::SMALL)
                               .arg(ui::fonts::DATA_FAMILY));
    mo_vl->addWidget(mo_desc);

    auto* mo_model = new QComboBox(mo_w);
    mo_model->addItems({"lightgbm", "xgboost", "lstm", "gru", "transformer", "tcn"});
    mo_model->setStyleSheet(combo_ss());
    mo_vl->addWidget(build_input_row("Model Type", mo_model, mo_w));

    auto* mo_target = new QComboBox(mo_w);
    mo_target->addItems({"sharpe", "ic", "max_drawdown", "win_rate", "calmar_ratio"});
    mo_target->setStyleSheet(combo_ss());
    mo_vl->addWidget(build_input_row("Optimize For", mo_target, mo_w));

    auto* mo_iter = new QSpinBox(mo_w);
    mo_iter->setRange(1, 100);
    mo_iter->setValue(10);
    mo_iter->setStyleSheet(input_ss());
    mo_vl->addWidget(build_input_row("Max Iterations", mo_iter, mo_w));

    auto* mo_run = make_run_button("START MODEL OPTIMIZATION", mo_w);
    connect(mo_run, &QPushButton::clicked, this, [this, mo_model, mo_target, mo_iter, status_txt, llm_combo]() {
        auto cfg = llm_config_from_combo(llm_combo);
        if (cfg.isEmpty()) {
            display_error("Select an LLM profile before running.");
            return;
        }
        status_label_->setText("Starting...");
        status_txt->setText("Model optimization started...");
        QJsonObject params;
        params["model_type"] = mo_model->currentText();
        params["optimization_target"] = mo_target->currentText();
        params["max_iterations"] = mo_iter->value();
        params["config"] = cfg;
        AIQuantLabService::instance().rd_agent_start_model_optimization(params);
    });
    mo_vl->addWidget(mo_run);
    mo_vl->addStretch();
    sub->addTab(mo_w, "Model Optimization");

    // ── Sub-tab 3: Quant Research ─────────────────────────────────────────────
    auto* qr_w = new QWidget(sub);
    auto* qr_vl = new QVBoxLayout(qr_w);
    qr_vl->setContentsMargins(14, 14, 14, 14);
    qr_vl->setSpacing(10);

    auto* qr_desc = new QLabel("Combined factor discovery + model optimization via QuantRDLoop. Runs the full "
                               "research pipeline end-to-end: propose factors, code them, backtest, refine.",
                               qr_w);
    qr_desc->setWordWrap(true);
    qr_desc->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3;")
                               .arg(ui::colors::TEXT_SECONDARY())
                               .arg(ui::fonts::SMALL)
                               .arg(ui::fonts::DATA_FAMILY));
    qr_vl->addWidget(qr_desc);

    auto* qr_goal = new QTextEdit(qr_w);
    qr_goal->setPlaceholderText(
        "Research goal...\ne.g. \"Build a quantitative equity strategy for mid-cap US stocks\"");
    qr_goal->setFixedHeight(70);
    qr_goal->setStyleSheet(input_ss());
    qr_vl->addWidget(build_input_row("Research Goal", qr_goal, qr_w));

    auto* qr_market = new QComboBox(qr_w);
    qr_market->addItems({"US", "CN", "EU", "IN", "JP", "HK", "GLOBAL"});
    qr_market->setStyleSheet(combo_ss());
    qr_vl->addWidget(build_input_row("Target Market", qr_market, qr_w));

    auto* qr_iter = new QSpinBox(qr_w);
    qr_iter->setRange(1, 100);
    qr_iter->setValue(15);
    qr_iter->setStyleSheet(input_ss());
    qr_vl->addWidget(build_input_row("Max Iterations", qr_iter, qr_w));

    auto* qr_run = make_run_button("START QUANT RESEARCH", qr_w);
    connect(qr_run, &QPushButton::clicked, this, [this, qr_goal, qr_market, qr_iter, status_txt, llm_combo]() {
        auto goal = qr_goal->toPlainText().trimmed();
        if (goal.isEmpty()) {
            display_error("Enter a research goal.");
            return;
        }
        auto cfg = llm_config_from_combo(llm_combo);
        if (cfg.isEmpty()) {
            display_error("Select an LLM profile before running.");
            return;
        }
        status_label_->setText("Starting...");
        status_txt->setText("Quant research started...");
        QJsonObject params;
        params["research_goal"] = goal;
        params["target_market"] = qr_market->currentText();
        params["max_iterations"] = qr_iter->value();
        params["config"] = cfg;
        AIQuantLabService::instance().rd_agent_start_quant_research(params);
    });
    qr_vl->addWidget(qr_run);
    qr_vl->addStretch();
    sub->addTab(qr_w, "Quant Research");

    // ── Sub-tab 4: Task Monitor ───────────────────────────────────────────────
    auto* tm_w = new QWidget(sub);
    auto* tm_vl = new QVBoxLayout(tm_w);
    tm_vl->setContentsMargins(14, 14, 14, 14);
    tm_vl->setSpacing(8);

    // Toolbar: filter + buttons
    auto* tm_toolbar = new QWidget(tm_w);
    auto* tm_hl = new QHBoxLayout(tm_toolbar);
    tm_hl->setContentsMargins(0, 0, 0, 0);
    tm_hl->setSpacing(6);

    auto* filter_combo = new QComboBox(tm_toolbar);
    filter_combo->addItems({"All", "running", "completed", "stopped", "failed"});
    filter_combo->setFixedWidth(120);
    filter_combo->setStyleSheet(combo_ss());
    tm_hl->addWidget(filter_combo);

    auto* refresh_btn = new QPushButton("REFRESH", tm_toolbar);
    refresh_btn->setCursor(Qt::PointingHandCursor);
    refresh_btn->setStyleSheet(
        QString("QPushButton { background:%1; color:%2; border:none;"
                "font-family:%3; font-size:%4px; font-weight:700; padding:5px 12px; border-radius:2px; }"
                "QPushButton:hover { background:%5; }")
            .arg(module_.color.name())
            .arg(ui::colors::BG_BASE())
            .arg(ui::fonts::DATA_FAMILY)
            .arg(ui::fonts::TINY)
            .arg(module_.color.lighter(120).name()));
    tm_hl->addWidget(refresh_btn);

    auto* task_id_input = new QLineEdit(tm_toolbar);
    task_id_input->setPlaceholderText("Task ID...");
    task_id_input->setStyleSheet(input_ss());
    task_id_input->setFixedWidth(200);
    tm_hl->addWidget(task_id_input);

    auto* stop_btn = new QPushButton("STOP", tm_toolbar);
    stop_btn->setCursor(Qt::PointingHandCursor);
    stop_btn->setStyleSheet(
        QString("QPushButton { background:" + QString(ui::colors::NEGATIVE()) +
                "; color:" + QString(ui::colors::TEXT_PRIMARY()) +
                "; border:none;"
                "font-family:%1; font-size:%2px; font-weight:700; padding:5px 12px; border-radius:2px; }"
                "QPushButton:hover { background:" +
                QString(ui::colors::NEGATIVE()) + "; }")
            .arg(ui::fonts::DATA_FAMILY)
            .arg(ui::fonts::TINY));
    tm_hl->addWidget(stop_btn);

    auto* resume_btn = new QPushButton("RESUME", tm_toolbar);
    resume_btn->setCursor(Qt::PointingHandCursor);
    resume_btn->setStyleSheet(
        QString("QPushButton { background:" + QString(ui::colors::POSITIVE()) +
                "; color:" + QString(ui::colors::TEXT_PRIMARY()) +
                "; border:none;"
                "font-family:%1; font-size:%2px; font-weight:700; padding:5px 12px; border-radius:2px; }"
                "QPushButton:hover { background:" +
                QString(ui::colors::POSITIVE()) + "; }")
            .arg(ui::fonts::DATA_FAMILY)
            .arg(ui::fonts::TINY));
    tm_hl->addWidget(resume_btn);

    auto* factors_btn = new QPushButton("GET FACTORS", tm_toolbar);
    factors_btn->setCursor(Qt::PointingHandCursor);
    factors_btn->setStyleSheet(
        QString("QPushButton { background:transparent; color:%1; border:1px solid %1;"
                "font-family:%2; font-size:%3px; font-weight:700; padding:5px 10px; border-radius:2px; }"
                "QPushButton:hover { background:%1; color:%4; }")
            .arg(module_.color.name())
            .arg(ui::fonts::DATA_FAMILY)
            .arg(ui::fonts::TINY)
            .arg(ui::colors::BG_BASE()));
    tm_hl->addWidget(factors_btn);
    tm_hl->addStretch();
    tm_vl->addWidget(tm_toolbar);

    // Task table
    rd_task_table_ = new QTableWidget(0, 6, tm_w);
    rd_task_table_->setHorizontalHeaderLabels({"Task ID", "Type", "Status", "Progress", "Best IC", "Elapsed"});
    rd_task_table_->horizontalHeader()->setStretchLastSection(true);
    rd_task_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    rd_task_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    rd_task_table_->setAlternatingRowColors(true);
    rd_task_table_->setStyleSheet(table_ss());
    tm_vl->addWidget(rd_task_table_, 1);

    // Output area for factor results
    rd_agent_output_ = new QTextEdit(tm_w);
    rd_agent_output_->setReadOnly(true);
    rd_agent_output_->setPlaceholderText("Select a task and click GET FACTORS / GET MODEL to view results...");
    rd_agent_output_->setFixedHeight(160);
    rd_agent_output_->setStyleSheet(output_ss());
    tm_vl->addWidget(rd_agent_output_);

    // Wire toolbar buttons
    connect(refresh_btn, &QPushButton::clicked, this, [filter_combo]() {
        auto filter = filter_combo->currentText();
        AIQuantLabService::instance().rd_agent_list_tasks(filter == "All" ? QString{} : filter);
    });

    connect(stop_btn, &QPushButton::clicked, this, [this, task_id_input]() {
        auto tid = task_id_input->text().trimmed();
        if (tid.isEmpty() && rd_task_table_->currentRow() >= 0)
            tid = rd_task_table_->item(rd_task_table_->currentRow(), 0)->text();
        if (tid.isEmpty()) {
            display_error("Select or enter a task ID.");
            return;
        }
        AIQuantLabService::instance().rd_agent_stop_task(tid);
    });

    connect(resume_btn, &QPushButton::clicked, this, [this, task_id_input]() {
        auto tid = task_id_input->text().trimmed();
        if (tid.isEmpty() && rd_task_table_->currentRow() >= 0)
            tid = rd_task_table_->item(rd_task_table_->currentRow(), 0)->text();
        if (tid.isEmpty()) {
            display_error("Select or enter a task ID.");
            return;
        }
        AIQuantLabService::instance().rd_agent_resume_task(tid);
    });

    connect(factors_btn, &QPushButton::clicked, this, [this, task_id_input]() {
        auto tid = task_id_input->text().trimmed();
        if (tid.isEmpty() && rd_task_table_->currentRow() >= 0)
            tid = rd_task_table_->item(rd_task_table_->currentRow(), 0)->text();
        if (tid.isEmpty()) {
            display_error("Select or enter a task ID.");
            return;
        }
        AIQuantLabService::instance().rd_agent_get_discovered_factors(tid);
    });

    // Populate task_id_input on row click
    connect(rd_task_table_, &QTableWidget::currentCellChanged, this, [task_id_input, this](int row, int, int, int) {
        if (row >= 0 && rd_task_table_->item(row, 0))
            task_id_input->setText(rd_task_table_->item(row, 0)->text());
    });

    sub->addTab(tm_w, "Task Monitor");

    vl->addWidget(sub, 1);
    return w;
}

} // namespace fincept::screens
