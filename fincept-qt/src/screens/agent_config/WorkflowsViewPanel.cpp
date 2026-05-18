// src/screens/agent_config/WorkflowsViewPanel.cpp
#include "screens/agent_config/WorkflowsViewPanel.h"

#include "services/llm/LlmService.h"
#include "core/logging/Logger.h"
#include "services/agents/AgentService.h"
#include "storage/repositories/LlmProfileRepository.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

#include <QHBoxLayout>
#include <QJsonObject>
#include <QShowEvent>
#include <QSplitter>
#include <QVBoxLayout>

namespace fincept::screens {

// ── Workflow catalog definition ───────────────────────────────────────────────

struct WorkflowDef {
    const char* id;
    const char* label;
    const char* category;
    const char* desc;
    bool needs_symbol;
    bool needs_query;
};

static const WorkflowDef kWorkflows[] = {
    {"stock_analysis", "Stock Analysis", "RESEARCH",
     "Full analysis of a stock: fundamentals, technicals, sentiment, and analyst targets.", true, false},
    {"portfolio_rebal", "Portfolio Rebalancing", "PORTFOLIO",
     "Analyse current holdings, suggest optimal allocation and rebalancing trades.", false, false},
    {"risk_assessment", "Risk Assessment", "PORTFOLIO",
     "Compute VaR, CVaR, max drawdown, stress-test scenarios across the portfolio.", false, false},
    {"execute_multi_query", "Multi-Agent Query", "EXECUTION",
     "Fan a query out to multiple agents in parallel and aggregate their responses.", false, true},
    {"macro_scan", "Macro Scanner", "RESEARCH",
     "Scan macro indicators — CPI, GDP, yield curves — and produce a market brief.", false, false},
    {"earnings_brief", "Earnings Brief", "RESEARCH",
     "Pull the latest earnings release for a ticker and generate an analyst brief.", true, false},
    {"sector_rotation", "Sector Rotation", "RESEARCH",
     "Identify momentum shifts across sectors and recommend rotation trades.", false, false},
    {"options_scan", "Options Flow Scan", "TRADING",
     "Scan unusual options activity and surface directional signals. Symbol optional.", false, false},
    {"sentiment_scan", "Sentiment Scan", "RESEARCH",
     "Aggregate news and social sentiment for a ticker over the past 48 hours.", true, false},
    {"custom_query", "Custom Query", "CUSTOM",
     "Send a free-form query through the workflow engine with the selected LLM profile.", false, true},
};

static constexpr int kWorkflowCount = static_cast<int>(sizeof(kWorkflows) / sizeof(kWorkflows[0]));

// ── Constructor ──────────────────────────────────────────────────────────────

WorkflowsViewPanel::WorkflowsViewPanel(QWidget* parent) : QWidget(parent) {
    setObjectName("WorkflowsViewPanel");
    build_ui();
    setup_connections();
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::theme_changed, this,
            [this](const ui::ThemeTokens&) { update(); });
}

// ── Layout ───────────────────────────────────────────────────────────────────

void WorkflowsViewPanel::build_ui() {
    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* sp = new QSplitter(Qt::Horizontal);
    sp->setHandleWidth(1);
    sp->setStyleSheet(QString("QSplitter::handle{background:%1;}").arg(ui::colors::BORDER_DIM()));
    sp->addWidget(build_catalog_panel());
    sp->addWidget(build_params_panel());
    sp->addWidget(build_output_panel());
    sp->setSizes({200, 320, 400});
    sp->setStretchFactor(0, 0);
    sp->setStretchFactor(1, 0);
    sp->setStretchFactor(2, 1);
    root->addWidget(sp);
}

// ── Left: workflow catalog ───────────────────────────────────────────────────

QWidget* WorkflowsViewPanel::build_catalog_panel() {
    auto* p = new QWidget(this);
    p->setMinimumWidth(170);
    p->setMaximumWidth(250);
    p->setStyleSheet(
        QString("background:%1;border-right:1px solid %2;").arg(ui::colors::BG_SURFACE(), ui::colors::BORDER_DIM()));
    auto* vl = new QVBoxLayout(p);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    // Header
    auto* hdr = new QWidget(this);
    hdr->setFixedHeight(34);
    hdr->setStyleSheet(
        QString("background:%1;border-bottom:1px solid %2;").arg(ui::colors::BG_RAISED(), ui::colors::BORDER_DIM()));
    auto* hl = new QHBoxLayout(hdr);
    hl->setContentsMargins(10, 0, 10, 0);
    auto* t = new QLabel("WORKFLOWS");
    t->setStyleSheet(QString("color:%1;font-size:10px;font-weight:700;letter-spacing:1px;").arg(ui::colors::AMBER()));
    hl->addWidget(t);
    hl->addStretch();
    auto* cnt = new QLabel(QString::number(kWorkflowCount));
    cnt->setStyleSheet(
        QString("color:%1;font-size:10px;background:%2;padding:1px 6px;").arg(ui::colors::CYAN(), ui::colors::BG_BASE()));
    hl->addWidget(cnt);
    vl->addWidget(hdr);

    // List
    catalog_list_ = new QListWidget;
    catalog_list_->setStyleSheet(QString("QListWidget{background:%1;border:none;color:%2;font-size:11px;outline:none;}"
                                         "QListWidget::item{padding:7px 10px;border-bottom:1px solid %3;}"
                                         "QListWidget::item:selected{background:%4;color:%2;border-left:2px solid %5;}"
                                         "QListWidget::item:hover{background:%6;}")
                                     .arg(ui::colors::BG_SURFACE(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_DIM(),
                                          ui::colors::BG_HOVER(), ui::colors::AMBER(), ui::colors::BG_HOVER()));

    // Group by category — insert dim category headers
    QString last_cat;
    for (int i = 0; i < kWorkflowCount; ++i) {
        const auto& wf = kWorkflows[i];
        if (QString(wf.category) != last_cat) {
            last_cat = wf.category;
            auto* sep = new QListWidgetItem(QString("  %1").arg(wf.category));
            sep->setFlags(Qt::NoItemFlags);
            sep->setForeground(QColor(ui::colors::TEXT_TERTIARY()));
            QFont f;
            f.setPointSize(8);
            f.setBold(true);
            sep->setFont(f);
            sep->setBackground(QColor(ui::colors::BG_RAISED()));
            catalog_list_->addItem(sep);
        }
        auto* item = new QListWidgetItem(QString("  %1").arg(wf.label));
        item->setData(Qt::UserRole, QString(wf.id));
        catalog_list_->addItem(item);
    }
    vl->addWidget(catalog_list_, 1);

    // Description footer
    wf_desc_label_ = new QLabel("Select a workflow to configure and run.");
    wf_desc_label_->setWordWrap(true);
    wf_desc_label_->setContentsMargins(10, 6, 10, 8);
    wf_desc_label_->setStyleSheet(QString("color:%1;font-size:10px;background:%2;border-top:1px solid %3;")
                                      .arg(ui::colors::TEXT_SECONDARY(), ui::colors::BG_RAISED(), ui::colors::BORDER_DIM()));
    vl->addWidget(wf_desc_label_);

    return p;
}

// ── Center: params ───────────────────────────────────────────────────────────

QWidget* WorkflowsViewPanel::build_params_panel() {
    auto* p = new QWidget(this);
    p->setStyleSheet(
        QString("background:%1;border-right:1px solid %2;").arg(ui::colors::BG_BASE(), ui::colors::BORDER_DIM()));
    auto* vl = new QVBoxLayout(p);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    // Header
    auto* hdr = new QWidget(this);
    hdr->setFixedHeight(34);
    hdr->setStyleSheet(
        QString("background:%1;border-bottom:1px solid %2;").arg(ui::colors::BG_RAISED(), ui::colors::BORDER_DIM()));
    auto* hl = new QHBoxLayout(hdr);
    hl->setContentsMargins(10, 0, 10, 0);
    params_title_ = new QLabel("PARAMETERS");
    params_title_->setStyleSheet(
        QString("color:%1;font-size:10px;font-weight:700;letter-spacing:1px;").arg(ui::colors::AMBER()));
    hl->addWidget(params_title_);
    vl->addWidget(hdr);

    // Body
    auto* body = new QWidget(this);
    auto* bl = new QVBoxLayout(body);
    bl->setContentsMargins(12, 10, 12, 12);
    bl->setSpacing(8);

    // LLM Profile
    auto* lbl_llm = new QLabel("LLM PROFILE");
    lbl_llm->setStyleSheet(
        QString("color:%1;font-size:10px;font-weight:700;letter-spacing:1px;").arg(ui::colors::TEXT_SECONDARY()));
    bl->addWidget(lbl_llm);

    llm_profile_combo_ = new QComboBox;
    llm_profile_combo_->setToolTip("LLM profile used by this workflow run");
    llm_profile_combo_->setStyleSheet(
        QString("QComboBox{background:%1;color:%2;border:1px solid %3;padding:3px 6px;font-size:11px;}"
                "QComboBox::drop-down{border:none;}"
                "QComboBox QAbstractItemView{background:%1;color:%2;selection-background-color:%4;}")
            .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_MED(), ui::colors::AMBER_DIM()));
    llm_profile_combo_->addItem("Default (Global)", QString{});
    const auto pr = LlmProfileRepository::instance().list_profiles();
    const auto profiles = pr.is_ok() ? pr.value() : QVector<LlmProfile>{};
    for (const auto& prof : profiles)
        llm_profile_combo_->addItem(prof.is_default ? prof.name + " [default]" : prof.name, prof.id);
    bl->addWidget(llm_profile_combo_);

    llm_resolved_lbl_ = new QLabel;
    llm_resolved_lbl_->setStyleSheet(QString("color:%1;font-size:10px;padding:1px 0;").arg(ui::colors::TEXT_TERTIARY()));
    bl->addWidget(llm_resolved_lbl_);

    // Symbol row (shown for symbol-requiring workflows)
    symbol_row_ = new QWidget(this);
    {
        auto* sl = new QHBoxLayout(symbol_row_);
        sl->setContentsMargins(0, 0, 0, 0);
        sl->setSpacing(6);
        auto* lbl = new QLabel("SYMBOL");
        lbl->setStyleSheet(QString("color:%1;font-size:10px;font-weight:700;letter-spacing:1px;min-width:55px;")
                               .arg(ui::colors::TEXT_SECONDARY()));
        sl->addWidget(lbl);
        symbol_input_ = new QLineEdit;
        symbol_input_->setPlaceholderText("e.g. AAPL");
        symbol_input_->setMaximumWidth(110);
        symbol_input_->setStyleSheet(
            QString("background:%1;color:%2;border:1px solid %3;padding:3px 6px;font-size:12px;")
                .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_MED()));
        sl->addWidget(symbol_input_);
        sl->addStretch();
    }
    symbol_row_->setVisible(false);
    bl->addWidget(symbol_row_);

    // Query row (shown for query-requiring workflows)
    query_row_ = new QWidget(this);
    {
        auto* ql = new QVBoxLayout(query_row_);
        ql->setContentsMargins(0, 0, 0, 0);
        ql->setSpacing(4);
        auto* lbl = new QLabel("QUERY");
        lbl->setStyleSheet(
            QString("color:%1;font-size:10px;font-weight:700;letter-spacing:1px;").arg(ui::colors::TEXT_SECONDARY()));
        ql->addWidget(lbl);
        query_input_ = new QTextEdit;
        query_input_->setPlaceholderText("Enter query for this workflow...");
        query_input_->setFixedHeight(90);
        query_input_->setStyleSheet(
            QString("QTextEdit{background:%1;color:%2;border:1px solid %3;padding:6px;font-size:12px;}")
                .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_MED()));
        ql->addWidget(query_input_);
    }
    query_row_->setVisible(false);
    bl->addWidget(query_row_);

    bl->addStretch();

    // Run button
    run_btn_ = new QPushButton("RUN WORKFLOW");
    run_btn_->setCursor(Qt::PointingHandCursor);
    run_btn_->setEnabled(false);
    run_btn_->setStyleSheet(QString("QPushButton{background:%1;color:%2;border:none;padding:9px;"
                                    "font-size:11px;font-weight:700;letter-spacing:1px;}"
                                    "QPushButton:hover{background:%3;}"
                                    "QPushButton:disabled{background:%4;color:%5;}")
                                .arg(ui::colors::AMBER(), ui::colors::BG_BASE(), ui::colors::ORANGE(), ui::colors::BG_RAISED(),
                                     ui::colors::TEXT_TERTIARY()));
    connect(run_btn_, &QPushButton::clicked, this, &WorkflowsViewPanel::run_current_workflow);
    bl->addWidget(run_btn_);

    params_status_ = new QLabel;
    params_status_->setStyleSheet(QString("color:%1;font-size:10px;padding:2px 0;").arg(ui::colors::TEXT_TERTIARY()));
    bl->addWidget(params_status_);

    vl->addWidget(body, 1);
    return p;
}

// ── Right: output panel ──────────────────────────────────────────────────────

QWidget* WorkflowsViewPanel::build_output_panel() {
    auto* p = new QWidget(this);
    p->setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE()));
    auto* vl = new QVBoxLayout(p);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    // Header
    auto* hdr = new QWidget(this);
    hdr->setFixedHeight(34);
    hdr->setStyleSheet(
        QString("background:%1;border-bottom:1px solid %2;").arg(ui::colors::BG_RAISED(), ui::colors::BORDER_DIM()));
    auto* hl = new QHBoxLayout(hdr);
    hl->setContentsMargins(10, 0, 10, 0);
    output_title_ = new QLabel("OUTPUT");
    output_title_->setStyleSheet(
        QString("color:%1;font-size:10px;font-weight:700;letter-spacing:1px;").arg(ui::colors::TEXT_SECONDARY()));
    hl->addWidget(output_title_);
    hl->addStretch();
    output_status_ = new QLabel;
    output_status_->setStyleSheet(QString("color:%1;font-size:10px;").arg(ui::colors::TEXT_TERTIARY()));
    hl->addWidget(output_status_);
    vl->addWidget(hdr);

    // Body
    auto* body = new QWidget(this);
    auto* bl = new QVBoxLayout(body);
    bl->setContentsMargins(10, 8, 10, 10);
    bl->setSpacing(6);

    auto* log_lbl = new QLabel("EXECUTION LOG");
    log_lbl->setStyleSheet(
        QString("color:%1;font-size:10px;font-weight:700;letter-spacing:1px;").arg(ui::colors::TEXT_SECONDARY()));
    bl->addWidget(log_lbl);

    log_display_ = new QTextEdit;
    log_display_->setReadOnly(true);
    log_display_->setFixedHeight(110);
    log_display_->setStyleSheet(
        QString("QTextEdit{background:%1;color:%2;border:1px solid %3;padding:6px;font-size:11px;}"
                "QScrollBar:vertical{background:%1;width:5px;border:none;}"
                "QScrollBar::handle:vertical{background:%4;min-height:20px;}"
                "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}")
            .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_SECONDARY(), ui::colors::BORDER_DIM(), ui::colors::BORDER_BRIGHT()));
    bl->addWidget(log_display_);

    auto* res_lbl = new QLabel("RESULT");
    res_lbl->setStyleSheet(QString("color:%1;font-size:10px;font-weight:700;letter-spacing:1px;padding-top:4px;")
                               .arg(ui::colors::TEXT_SECONDARY()));
    bl->addWidget(res_lbl);

    result_display_ = new QTextEdit;
    result_display_->setReadOnly(true);
    result_display_->setStyleSheet(
        QString("QTextEdit{background:%1;color:%2;border:1px solid %3;padding:8px;font-size:12px;}"
                "QScrollBar:vertical{background:%1;width:5px;border:none;}"
                "QScrollBar::handle:vertical{background:%4;min-height:20px;}"
                "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}")
            .arg(ui::colors::BG_BASE(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_DIM(), ui::colors::BORDER_BRIGHT()));
    result_display_->document()->setDefaultStyleSheet(
        QString("body{color:%1;background:%2;font-size:12px;}"
                "h1,h2,h3{color:%3;margin:6px 0 3px 0;}"
                "code{background:%4;color:%1;padding:1px 4px;font-family:monospace;}"
                "pre{background:%4;color:%1;padding:6px;font-family:monospace;}"
                "a{color:%5;} ul,ol{margin:3px 0;padding-left:18px;}"
                "li{margin:2px 0;} strong{color:%1;}"
                "hr{border:none;border-top:1px solid %6;}")
            .arg(ui::colors::TEXT_PRIMARY(), ui::colors::BG_BASE(), ui::colors::AMBER(), ui::colors::BG_RAISED(),
                 ui::colors::CYAN(), ui::colors::BORDER_DIM()));
    bl->addWidget(result_display_, 1);

    vl->addWidget(body, 1);
    return p;
}

// ── Connections ──────────────────────────────────────────────────────────────

void WorkflowsViewPanel::setup_connections() {
    connect(catalog_list_, &QListWidget::currentRowChanged, this, &WorkflowsViewPanel::on_workflow_selected);

    auto& svc = services::AgentService::instance();

    connect(&svc, &services::AgentService::agent_result, this, [this](services::AgentExecutionResult r) {
        if (r.request_id != pending_request_id_)
            return;
        executing_ = false;
        pending_request_id_.clear();
        run_btn_->setEnabled(true);
        run_btn_->setText("RUN WORKFLOW");
        if (r.success) {
            result_display_->setMarkdown(r.response);
            const QString msg = QString("Done in %1ms").arg(r.execution_time_ms);
            output_status_->setText(msg);
            output_status_->setStyleSheet(QString("color:%1;font-size:10px;").arg(ui::colors::POSITIVE()));
            log_display_->append(QString("[DONE] %1").arg(msg));
        } else {
            result_display_->setPlainText("Error: " + r.error);
            output_status_->setText("FAILED");
            output_status_->setStyleSheet(QString("color:%1;font-size:10px;").arg(ui::colors::NEGATIVE()));
            log_display_->append("[ERROR] " + r.error);
        }
    });

    connect(&svc, &services::AgentService::agent_stream_thinking, this,
            [this](const QString& request_id, const QString& status) {
                if (request_id != pending_request_id_)
                    return;
                output_status_->setText(status);
                log_display_->append("[THINK] " + status);
            });

    connect(&svc, &services::AgentService::agent_stream_token, this,
            [this](const QString& request_id, const QString& token) {
                if (request_id != pending_request_id_)
                    return;
                QTextCursor cursor = result_display_->textCursor();
                cursor.movePosition(QTextCursor::End);
                result_display_->setTextCursor(cursor);
                result_display_->insertPlainText(token + " ");
            });

    connect(&svc, &services::AgentService::agent_stream_done, this, [this](services::AgentExecutionResult r) {
        if (r.request_id != pending_request_id_)
            return;
        executing_ = false;
        pending_request_id_.clear();
        run_btn_->setEnabled(true);
        run_btn_->setText("RUN WORKFLOW");
        if (r.success) {
            result_display_->setMarkdown(r.response);
            const QString msg = QString("Done in %1ms").arg(r.execution_time_ms);
            output_status_->setText(msg);
            output_status_->setStyleSheet(QString("color:%1;font-size:10px;").arg(ui::colors::POSITIVE()));
            log_display_->append(QString("[DONE] %1").arg(msg));
        } else {
            result_display_->setPlainText("Error: " + r.error);
            output_status_->setText("FAILED");
            output_status_->setStyleSheet(QString("color:%1;font-size:10px;").arg(ui::colors::NEGATIVE()));
            log_display_->append("[ERROR] " + r.error);
        }
    });

    connect(&svc, &services::AgentService::error_occurred, this, [this](const QString&, const QString& msg) {
        if (!executing_)
            return;
        executing_ = false;
        pending_request_id_.clear();
        run_btn_->setEnabled(true);
        run_btn_->setText("RUN WORKFLOW");
        result_display_->setPlainText("Error: " + msg);
        output_status_->setText("ERROR");
        output_status_->setStyleSheet(QString("color:%1;font-size:10px;").arg(ui::colors::NEGATIVE()));
        log_display_->append("[ERROR] " + msg);
    });

    connect(llm_profile_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() {
        const QString pid = llm_profile_combo_->currentData().toString();
        if (pid.isEmpty()) {
            auto resolved = ai_chat::LlmService::instance().resolve_profile("workflow", {});
            if (!resolved.provider.isEmpty())
                llm_resolved_lbl_->setText(resolved.provider.toUpper() + " / " + resolved.model_id + " (inherited)");
            else
                llm_resolved_lbl_->setText("No provider — Settings > LLM Config");
        } else {
            const auto pr2 = LlmProfileRepository::instance().list_profiles();
            const auto profs = pr2.is_ok() ? pr2.value() : QVector<LlmProfile>{};
            for (const auto& p : profs) {
                if (p.id == pid) {
                    llm_resolved_lbl_->setText(p.provider.toUpper() + " / " + p.model_id);
                    break;
                }
            }
        }
    });
    // Trigger once to populate resolved label
    emit llm_profile_combo_->currentIndexChanged(0);
}

// ── Workflow selection ───────────────────────────────────────────────────────

void WorkflowsViewPanel::on_workflow_selected(int row) {
    if (row < 0)
        return;
    auto* item = catalog_list_->item(row);
    if (!item || !(item->flags() & Qt::ItemIsEnabled))
        return;

    const QString wf_id = item->data(Qt::UserRole).toString();
    if (wf_id.isEmpty())
        return;

    current_workflow_type_ = wf_id;

    // Find the def
    const WorkflowDef* def = nullptr;
    for (int i = 0; i < kWorkflowCount; ++i) {
        if (QString(kWorkflows[i].id) == wf_id) {
            def = &kWorkflows[i];
            break;
        }
    }
    if (!def)
        return;

    params_title_->setText(QString("%1  —  PARAMETERS").arg(def->label).toUpper());
    wf_desc_label_->setText(def->desc);

    symbol_row_->setVisible(def->needs_symbol);
    query_row_->setVisible(def->needs_query);

    run_btn_->setEnabled(true);
    params_status_->clear();
}

// ── Run ──────────────────────────────────────────────────────────────────────

void WorkflowsViewPanel::run_current_workflow() {
    if (executing_ || current_workflow_type_.isEmpty())
        return;

    executing_ = true;
    run_btn_->setEnabled(false);
    run_btn_->setText("RUNNING...");
    result_display_->clear();
    log_display_->clear();
    output_title_->setText(QString("OUTPUT  —  %1").arg(current_workflow_type_.toUpper()));
    output_status_->setText("Executing...");
    output_status_->setStyleSheet(QString("color:%1;font-size:10px;").arg(ui::colors::AMBER()));
    log_display_->append(QString("[START] %1").arg(current_workflow_type_));

    QJsonObject params;
    if (symbol_row_->isVisible()) {
        const QString sym = symbol_input_->text().trimmed().toUpper();
        if (sym.isEmpty()) {
            executing_ = false;
            run_btn_->setEnabled(true);
            run_btn_->setText("RUN WORKFLOW");
            params_status_->setText("Symbol is required");
            params_status_->setStyleSheet(QString("color:%1;font-size:10px;padding:2px 0;").arg(ui::colors::NEGATIVE()));
            return;
        }
        params["symbol"] = sym;
        log_display_->append(QString("[PARAM] symbol=%1").arg(sym));
    }
    if (query_row_->isVisible()) {
        const QString q = query_input_->toPlainText().trimmed();
        if (q.isEmpty()) {
            executing_ = false;
            run_btn_->setEnabled(true);
            run_btn_->setText("RUN WORKFLOW");
            params_status_->setText("Query is required");
            params_status_->setStyleSheet(QString("color:%1;font-size:10px;padding:2px 0;").arg(ui::colors::NEGATIVE()));
            return;
        }
        params["query"] = q;
    }

    const QString profile_id = llm_profile_combo_->currentData().toString();
    if (!profile_id.isEmpty())
        params["llm_profile_id"] = profile_id;

    params_status_->clear();
    pending_request_id_ = services::AgentService::instance().run_workflow(current_workflow_type_, params);
    log_display_->append(QString("[REQ] %1").arg(pending_request_id_.left(8)));
}

void WorkflowsViewPanel::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
}

} // namespace fincept::screens
