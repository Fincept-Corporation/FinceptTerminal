// src/screens/agent_config/WorkflowsViewPanel.cpp
#include "screens/agent_config/WorkflowsViewPanel.h"

#include "core/logging/Logger.h"
#include "screens/agent_config/AgentProfileRows.h"
#include "screens/agent_config/AgentResultFormat.h"
#include "services/agents/AgentService.h"
#include "services/llm/LlmService.h"
#include "storage/repositories/LlmProfileRepository.h"
#include "storage/repositories/PortfolioRepository.h"
#include "ui/theme/Theme.h"
#include "services/instruments/InstrumentResolver.h"
#include "ui/theme/ThemeManager.h"
#include "ui/widgets/EmptyState.h"
#include "ui/widgets/symbol/SymbolSearchBox.h"

#include <QCoreApplication>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QJsonArray>
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
     "Route a query to the best-matching agents, run them one after another and aggregate their responses.",
     false, true},
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

// Validation-message styling in one place (was two identical inline setStyleSheet pairs).
static void set_params_status(QLabel* lbl, const QString& text, const QString& color) {
    lbl->setText(text);
    lbl->setStyleSheet(QString("color:%1;font-size:" + fincept::ui::fonts::px(-4) + "px;padding:2px 0;").arg(color));
}

// A category header row of the catalog: small bold text on the raised fill.
static void style_category_header(QListWidgetItem* sep) {
    sep->setForeground(QColor(ui::colors::TEXT_SECONDARY()));
    sep->setFont(ui::fonts::font(-3, QFont::Bold));
    sep->setBackground(QColor(ui::colors::BG_RAISED()));
}

// Default stylesheet of the result document (rendered markdown / HTML).
static QString result_doc_css() {
    return QString("body{color:%1;background:%2;font-size:" + fincept::ui::fonts::px(-2) + "px;}"
                   "h1,h2,h3{color:%3;margin:6px 0 3px 0;}"
                   "code{background:%4;color:%1;padding:1px 4px;}"
                   "pre{background:%4;color:%1;padding:6px;}"
                   "a{color:%5;} ul,ol{margin:3px 0;padding-left:18px;}"
                   "li{margin:2px 0;} strong{color:%1;}"
                   "hr{border:none;border-top:1px solid %6;}")
        .arg(ui::colors::TEXT_PRIMARY(), ui::colors::BG_BASE(), ui::colors::AMBER(), ui::colors::BG_RAISED(),
             ui::colors::CYAN(), ui::colors::BORDER_DIM());
}

// Workflows that analyse a user portfolio (and so offer the PORTFOLIO picker).
static bool workflow_uses_portfolio(const QString& id) {
    return id == QLatin1String("portfolio_rebal") || id == QLatin1String("risk_assessment");
}

// ── Constructor ──────────────────────────────────────────────────────────────

WorkflowsViewPanel::WorkflowsViewPanel(QWidget* parent) : QWidget(parent) {
    setObjectName("WorkflowsViewPanel");
    build_ui();
    setup_connections();
    // Open on the first workflow (skip the category header rows), so the
    // parameters and the result preview describe it instead of "Select …".
    for (int i = 0; i < catalog_list_->count(); ++i) {
        if (catalog_list_->item(i)->flags() & Qt::ItemIsEnabled) {
            catalog_list_->setCurrentRow(i); // -> on_workflow_selected(i)
            break;
        }
    }
    // Category header rows carry item brushes and a font; the result document
    // keeps its own stylesheet.
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::theme_changed, this, [this](const ui::ThemeTokens&) {
        for (int i = 0; i < catalog_list_->count(); ++i) {
            QListWidgetItem* item = catalog_list_->item(i);
            if (item->flags() == Qt::NoItemFlags)
                style_category_header(item);
        }
        result_display_->document()->setDefaultStyleSheet(result_doc_css());
        agent_result::rerender(result_display_); // its HTML carries the theme's colours
    });
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
    hdr->setMinimumHeight(34);
    hdr->setStyleSheet(
        QString("background:%1;border-bottom:1px solid %2;").arg(ui::colors::BG_RAISED(), ui::colors::BORDER_DIM()));
    auto* hl = new QHBoxLayout(hdr);
    hl->setContentsMargins(10, 0, 10, 0);
    catalog_title_ = new QLabel(tr("WORKFLOWS"));
    catalog_title_->setStyleSheet(
        QString("color:%1;font-size:" + fincept::ui::fonts::px(-4) + "px;font-weight:700;letter-spacing:1px;").arg(ui::colors::AMBER()));
    hl->addWidget(catalog_title_);
    hl->addStretch();
    auto* cnt = new QLabel(QString::number(kWorkflowCount));
    cnt->setStyleSheet(QString("color:%1;font-size:" + fincept::ui::fonts::px(-4) + "px;background:%2;padding:1px 6px;")
                           .arg(ui::colors::CYAN(), ui::colors::BG_BASE()));
    hl->addWidget(cnt);
    vl->addWidget(hdr);

    // List
    catalog_list_ = new QListWidget;
    catalog_list_->setStyleSheet(QString("QListWidget{background:%1;border:none;color:%2;font-size:" + fincept::ui::fonts::px(-3) + "px;outline:none;}"
                                         "QListWidget::item{padding:7px 10px;border-bottom:1px solid %3;}"
                                         "QListWidget::item:selected{background:%4;color:%2;border-left:2px solid %5;}"
                                         "QListWidget::item:hover{background:%6;}")
                                     .arg(ui::colors::BG_SURFACE(), ui::colors::TEXT_PRIMARY(),
                                          ui::colors::BORDER_DIM(), ui::colors::BG_HOVER(), ui::colors::AMBER(),
                                          ui::colors::BG_HOVER()));

    // Group by category — insert dim category headers.
    // Category + label come from the kWorkflows table; translate them through
    // the WorkflowsViewPanel context so they participate in i18n. The catalog is
    // built once at construction — a live language switch repopulates via
    // retranslateUi() (see rebuild note there).
    QString last_cat;
    for (int i = 0; i < kWorkflowCount; ++i) {
        const auto& wf = kWorkflows[i];
        if (QString(wf.category) != last_cat) {
            last_cat = wf.category;
            auto* sep = new QListWidgetItem(
                QString("  %1").arg(QCoreApplication::translate("WorkflowsViewPanel", wf.category)));
            sep->setFlags(Qt::NoItemFlags);
            style_category_header(sep);
            catalog_list_->addItem(sep);
        }
        auto* item =
            new QListWidgetItem(QString("  %1").arg(QCoreApplication::translate("WorkflowsViewPanel", wf.label)));
        item->setData(Qt::UserRole, QString(wf.id));
        catalog_list_->addItem(item);
    }
    vl->addWidget(catalog_list_, 1);

    // Description footer
    wf_desc_label_ = new QLabel(tr("Select a workflow to configure and run."));
    wf_desc_label_->setWordWrap(true);
    wf_desc_label_->setContentsMargins(10, 6, 10, 8);
    wf_desc_label_->setStyleSheet(
        QString("color:%1;font-size:" + fincept::ui::fonts::px(-4) + "px;background:%2;border-top:1px solid %3;")
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
    hdr->setMinimumHeight(34);
    hdr->setStyleSheet(
        QString("background:%1;border-bottom:1px solid %2;").arg(ui::colors::BG_RAISED(), ui::colors::BORDER_DIM()));
    auto* hl = new QHBoxLayout(hdr);
    hl->setContentsMargins(10, 0, 10, 0);
    params_title_ = new QLabel(tr("PARAMETERS"));
    params_title_->setStyleSheet(
        QString("color:%1;font-size:" + fincept::ui::fonts::px(-4) + "px;font-weight:700;letter-spacing:1px;").arg(ui::colors::AMBER()));
    hl->addWidget(params_title_);
    vl->addWidget(hdr);

    // Body
    auto* body = new QWidget(this);
    auto* bl = new QVBoxLayout(body);
    bl->setContentsMargins(12, 10, 12, 12);
    bl->setSpacing(8);

    // LLM Profile
    llm_profile_title_ = new QLabel(tr("LLM PROFILE"));
    llm_profile_title_->setStyleSheet(
        QString("color:%1;font-size:" + fincept::ui::fonts::px(-4) + "px;font-weight:700;letter-spacing:1px;").arg(ui::colors::TEXT_SECONDARY()));
    bl->addWidget(llm_profile_title_);

    llm_profile_combo_ = new QComboBox;
    llm_profile_combo_->setToolTip(tr("LLM profile used by this workflow run"));
    llm_profile_combo_->setStyleSheet(
        QString("QComboBox{background:%1;color:%2;border:1px solid %3;padding:3px 6px;font-size:" + fincept::ui::fonts::px(-3) + "px;}"
                "QComboBox::drop-down{border:none;}"
                "QComboBox QAbstractItemView{background:%1;color:%2;selection-background-color:%4;selection-color:%5;}")
            .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_MED(),
                 ui::colors::AMBER_DIM(), ui::colors::TEXT_ON_ACCENT()));
    llm_profile_combo_->addItem(tr("Default (Global)"), QString{});
    // Usable profiles, then harness profiles as disabled rows (no id: never chosen).
    agent_profiles::add_profile_rows(llm_profile_combo_, tr(" [default]"), QStringLiteral("workflow"));
    bl->addWidget(llm_profile_combo_);

    llm_resolved_lbl_ = new QLabel;
    llm_resolved_lbl_->setStyleSheet(
        QString("color:%1;font-size:" + fincept::ui::fonts::px(-4) + "px;padding:1px 0;").arg(ui::colors::TEXT_TERTIARY()));
    bl->addWidget(llm_resolved_lbl_);

    // Symbol row (shown for symbol-requiring workflows)
    symbol_row_ = new QWidget(this);
    {
        auto* sl = new QHBoxLayout(symbol_row_);
        sl->setContentsMargins(0, 0, 0, 0);
        sl->setSpacing(6);
        symbol_label_ = new QLabel(tr("SYMBOL"));
        symbol_label_->setStyleSheet(
            QString("color:%1;font-size:" + fincept::ui::fonts::px(-4) + "px;font-weight:700;letter-spacing:1px;min-width:55px;")
                .arg(ui::colors::TEXT_SECONDARY()));
        sl->addWidget(symbol_label_);
        // The shared symbol picker (search-as-you-type, never blocking). The workflow gets
        // the picked instrument's market-data symbol; typed text is resolved before a run.
        symbol_input_ = new ui::SymbolSearchBox(symbol_row_);
        symbol_input_->set_compact(true);
        symbol_input_->set_clear_on_pick(false);
        symbol_input_->set_recent_key(QStringLiteral("agent_workflows"));
        symbol_input_->set_placeholder(tr("Ticker or company, e.g. AAPL"));
        symbol_input_->setMinimumWidth(QFontMetrics(ui::fonts::font(-2)).horizontalAdvance(QString(16, QLatin1Char('M'))));
        sl->addWidget(symbol_input_, 1);
    }
    symbol_row_->setVisible(false);
    bl->addWidget(symbol_row_);

    // Portfolio row (portfolio rebalancing / risk assessment). Those workflows used to
    // run with no portfolio at all — finagent_core was handed this tab's bookkeeping
    // params as the "portfolio data". One stylesheet on the row styles label + combo.
    portfolio_row_ = new QWidget(this);
    portfolio_row_->setStyleSheet(
        QString("QLabel{color:%1;font-size:" + fincept::ui::fonts::px(-4) + "px;font-weight:700;letter-spacing:1px;min-width:55px;}"
                "QComboBox{background:%2;color:%3;border:1px solid %4;padding:3px 6px;font-size:" + fincept::ui::fonts::px(-3) + "px;}"
                "QComboBox::drop-down{border:none;}"
                "QComboBox QAbstractItemView{background:%2;color:%3;selection-background-color:%5;selection-color:%6;}")
            .arg(ui::colors::TEXT_SECONDARY(), ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(),
                 ui::colors::BORDER_MED(), ui::colors::AMBER_DIM(), ui::colors::TEXT_ON_ACCENT()));
    {
        auto* pl = new QHBoxLayout(portfolio_row_);
        pl->setContentsMargins(0, 0, 0, 0);
        pl->setSpacing(6);
        portfolio_label_ = new QLabel(tr("PORTFOLIO"));
        pl->addWidget(portfolio_label_);
        portfolio_combo_ = new QComboBox;
        portfolio_combo_->setToolTip(tr("Portfolio whose holdings the workflow analyses"));
        pl->addWidget(portfolio_combo_, 1);
    }
    portfolio_row_->setVisible(false);
    bl->addWidget(portfolio_row_);

    // Query row (shown for query-requiring workflows)
    query_row_ = new QWidget(this);
    {
        auto* ql = new QVBoxLayout(query_row_);
        ql->setContentsMargins(0, 0, 0, 0);
        ql->setSpacing(4);
        query_label_ = new QLabel(tr("QUERY"));
        query_label_->setStyleSheet(
            QString("color:%1;font-size:" + fincept::ui::fonts::px(-4) + "px;font-weight:700;letter-spacing:1px;").arg(ui::colors::TEXT_SECONDARY()));
        ql->addWidget(query_label_);
        query_input_ = new QTextEdit;
        query_input_->setPlaceholderText(tr("Enter query for this workflow..."));
        query_input_->setFixedHeight(90);
        query_input_->setStyleSheet(
            QString("QTextEdit{background:%1;color:%2;border:1px solid %3;padding:6px;font-size:" + fincept::ui::fonts::px(-2) + "px;}")
                .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_MED()));
        ql->addWidget(query_input_);
    }
    query_row_->setVisible(false);
    bl->addWidget(query_row_);

    bl->addStretch();

    // Run button
    run_btn_ = new QPushButton(tr("RUN WORKFLOW"));
    run_btn_->setProperty("fincept_agent_risk", "spend"); // agents: runs the workflow (model quota)
    run_btn_->setCursor(Qt::PointingHandCursor);
    run_btn_->setEnabled(false);
    run_btn_->setStyleSheet(QString("QPushButton{background:%1;color:%2;border:none;padding:9px;"
                                    "font-size:" + fincept::ui::fonts::px(-3) + "px;font-weight:700;letter-spacing:1px;}"
                                    "QPushButton:hover{background:%3;}"
                                    "QPushButton:disabled{background:%4;color:%5;}")
                                .arg(ui::colors::AMBER(), ui::colors::TEXT_ON_ACCENT(), ui::colors::AMBER_DIM(),
                                     ui::colors::BG_RAISED(), ui::colors::TEXT_TERTIARY()));
    connect(run_btn_, &QPushButton::clicked, this, &WorkflowsViewPanel::run_current_workflow);
    bl->addWidget(run_btn_);

    params_status_ = new QLabel;
    params_status_->setStyleSheet(QString("color:%1;font-size:" + fincept::ui::fonts::px(-4) + "px;padding:2px 0;").arg(ui::colors::TEXT_TERTIARY()));
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
    hdr->setMinimumHeight(34);
    hdr->setStyleSheet(
        QString("background:%1;border-bottom:1px solid %2;").arg(ui::colors::BG_RAISED(), ui::colors::BORDER_DIM()));
    auto* hl = new QHBoxLayout(hdr);
    hl->setContentsMargins(10, 0, 10, 0);
    output_title_ = new QLabel(tr("OUTPUT"));
    output_title_->setStyleSheet(
        QString("color:%1;font-size:" + fincept::ui::fonts::px(-4) + "px;font-weight:700;letter-spacing:1px;").arg(ui::colors::TEXT_SECONDARY()));
    hl->addWidget(output_title_);
    hl->addStretch();
    output_status_ = new QLabel;
    output_status_->setStyleSheet(QString("color:%1;font-size:" + fincept::ui::fonts::px(-4) + "px;").arg(ui::colors::TEXT_TERTIARY()));
    hl->addWidget(output_status_);
    vl->addWidget(hdr);

    // Body
    auto* body = new QWidget(this);
    auto* bl = new QVBoxLayout(body);
    bl->setContentsMargins(10, 8, 10, 10);
    bl->setSpacing(6);

    output_log_title_ = new QLabel(tr("EXECUTION LOG"));
    output_log_title_->setStyleSheet(
        QString("color:%1;font-size:" + fincept::ui::fonts::px(-4) + "px;font-weight:700;letter-spacing:1px;").arg(ui::colors::TEXT_SECONDARY()));
    bl->addWidget(output_log_title_);

    log_display_ = new QTextEdit;
    log_display_->setReadOnly(true);
    log_display_->setFixedHeight(110);
    log_display_->setStyleSheet(
        QString("QTextEdit{background:%1;color:%2;border:1px solid %3;padding:6px;font-size:" + fincept::ui::fonts::px(-3) + "px;}"
                "QScrollBar:vertical{background:%1;width:5px;border:none;}"
                "QScrollBar::handle:vertical{background:%4;min-height:20px;}"
                "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}")
            .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_SECONDARY(), ui::colors::BORDER_DIM(),
                 ui::colors::BORDER_BRIGHT()));
    bl->addWidget(log_display_);

    output_result_title_ = new QLabel(tr("RESULT"));
    output_result_title_->setStyleSheet(
        QString("color:%1;font-size:" + fincept::ui::fonts::px(-4) + "px;font-weight:700;letter-spacing:1px;padding-top:4px;")
            .arg(ui::colors::TEXT_SECONDARY()));
    bl->addWidget(output_result_title_);

    // Result, or — before the first run — what the selected workflow does. A run
    // calls the LLM (tokens on the user's provider): it never starts by itself.
    result_stack_ = new QStackedWidget(body);
    result_empty_ = new ui::EmptyState(result_stack_);
    result_empty_->set_compact(true);
    connect(result_empty_, &ui::EmptyState::primary_clicked, this, &WorkflowsViewPanel::run_current_workflow);
    result_stack_->addWidget(result_empty_);
    result_display_ = new QTextEdit(result_stack_);
    result_display_->setReadOnly(true);
    result_display_->setStyleSheet(
        QString("QTextEdit{background:%1;color:%2;border:1px solid %3;padding:8px;font-size:" + fincept::ui::fonts::px(-2) + "px;}"
                "QScrollBar:vertical{background:%1;width:5px;border:none;}"
                "QScrollBar::handle:vertical{background:%4;min-height:20px;}"
                "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}")
            .arg(ui::colors::BG_BASE(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_DIM(),
                 ui::colors::BORDER_BRIGHT()));
    result_display_->document()->setDefaultStyleSheet(result_doc_css());
    result_stack_->addWidget(result_display_);
    result_stack_->setCurrentWidget(result_empty_);
    update_result_preview(QString(), QString());
    bl->addWidget(result_stack_, 1);

    vl->addWidget(body, 1);
    return p;
}

void WorkflowsViewPanel::update_result_preview(const QString& label, const QString& desc) {
    if (!result_empty_)
        return;
    result_empty_->apply(ui::EmptyState::preview_spec(
        label.isEmpty() ? tr("Run a workflow") : tr("Run %1").arg(label),
        desc.isEmpty() ? tr("Pick a workflow on the left; its result appears here and its steps in the execution log.")
                       : desc,
        tr("RUN WORKFLOW"), tr("Uses your LLM provider — billed as tokens there"), ui::EmptyState::Tone::Warning,
        ui::LoadingSkeleton::Shape::Text));
    result_empty_->set_primary_enabled(!label.isEmpty());
}

// ── Connections ──────────────────────────────────────────────────────────────

void WorkflowsViewPanel::setup_connections() {
    connect(catalog_list_, &QListWidget::currentRowChanged, this, &WorkflowsViewPanel::on_workflow_selected);

    auto& svc = services::AgentService::instance();

    connect(&svc, &services::AgentService::agent_result, this, [this](services::AgentExecutionResult r) {
        // An idle panel (empty pending id) must not take results of other screens.
        if (pending_request_id_.isEmpty() || r.request_id != pending_request_id_)
            return;
        executing_ = false;
        pending_request_id_.clear();
        run_btn_->setEnabled(true);
        run_btn_->setText(tr("RUN WORKFLOW"));
        if (r.success) {
            agent_result::show_markdown(result_display_, r.response);
            const QString msg = tr("Done in %1").arg(agent_result::duration(r.execution_time_ms));
            output_status_->setText(msg);
            output_status_->setStyleSheet(QString("color:%1;font-size:" + fincept::ui::fonts::px(-4) + "px;").arg(ui::colors::SUCCESS()));
            log_display_->append(QString("[DONE] %1").arg(msg));
        } else {
            agent_result::show_plain(result_display_, tr("Error: %1").arg(r.error));
            output_status_->setText(tr("FAILED"));
            output_status_->setStyleSheet(QString("color:%1;font-size:" + fincept::ui::fonts::px(-4) + "px;").arg(ui::colors::DANGER()));
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
                result_display_->insertPlainText(token);
            });

    connect(&svc, &services::AgentService::agent_stream_done, this, [this](services::AgentExecutionResult r) {
        if (pending_request_id_.isEmpty() || r.request_id != pending_request_id_)
            return;
        executing_ = false;
        pending_request_id_.clear();
        run_btn_->setEnabled(true);
        run_btn_->setText(tr("RUN WORKFLOW"));
        if (r.success) {
            agent_result::show_markdown(result_display_, r.response);
            const QString msg = tr("Done in %1").arg(agent_result::duration(r.execution_time_ms));
            output_status_->setText(msg);
            output_status_->setStyleSheet(QString("color:%1;font-size:" + fincept::ui::fonts::px(-4) + "px;").arg(ui::colors::SUCCESS()));
            log_display_->append(QString("[DONE] %1").arg(msg));
        } else {
            agent_result::show_plain(result_display_, tr("Error: %1").arg(r.error));
            output_status_->setText(tr("FAILED"));
            output_status_->setStyleSheet(QString("color:%1;font-size:" + fincept::ui::fonts::px(-4) + "px;").arg(ui::colors::DANGER()));
            log_display_->append("[ERROR] " + r.error);
        }
    });

    // No AgentService::error_occurred handler: run_workflow always ends in
    // agent_result (failures included). error_occurred reports unrelated
    // discovery / task / schedule / plan / config calls, which used to flip a
    // running workflow to ERROR and then drop its real result.

    connect(llm_profile_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() {
        const QString pid = llm_profile_combo_->currentData().toString();
        if (pid.isEmpty()) {
            auto resolved = ai_chat::LlmService::instance().resolve_profile("workflow", {});
            if (!resolved.provider.isEmpty())
                llm_resolved_lbl_->setText(resolved.provider.toUpper() + " / " + resolved.model_id +
                                           tr(" (inherited)"));
            else
                llm_resolved_lbl_->setText(tr("No provider — Settings > LLM Config"));
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

    params_title_->setText(
        tr("%1  —  PARAMETERS").arg(QCoreApplication::translate("WorkflowsViewPanel", def->label)).toUpper());
    wf_desc_label_->setText(QCoreApplication::translate("WorkflowsViewPanel", def->desc));

    // options_scan takes an OPTIONAL symbol ("Symbol optional" in its description and
    // in finagent_core) — the field used to be hidden for it, so it could never be set.
    symbol_row_->setVisible(def->needs_symbol || wf_id == QLatin1String("options_scan"));
    query_row_->setVisible(def->needs_query);
    portfolio_row_->setVisible(workflow_uses_portfolio(wf_id));
    if (workflow_uses_portfolio(wf_id))
        refresh_portfolios();

    run_btn_->setEnabled(true);
    params_status_->clear();
    update_result_preview(QCoreApplication::translate("WorkflowsViewPanel", def->label),
                          QCoreApplication::translate("WorkflowsViewPanel", def->desc));
}

// ── Run ──────────────────────────────────────────────────────────────────────

void WorkflowsViewPanel::run_current_workflow() {
    if (executing_ || current_workflow_type_.isEmpty() || symbol_resolve_id_ != 0)
        return;

    // Typed, not picked: resolve the symbol first (asynchronously), then run with the
    // instrument's market-data symbol — "RELIANCE" quotes nothing without ".NS", "TCS" is
    // also TECSYS on the TSX. Ambiguous / unknown text asks for a pick. Checked before
    // anything on screen changes, so a failed lookup leaves the last result in place.
    if (symbol_row_->isVisible()) {
        const auto picked = symbol_input_->current_instrument();
        const QString typed = symbol_input_->text().trimmed();
        if (!typed.isEmpty() && !(picked.is_valid() && !picked.symbol.isEmpty())) {
            run_btn_->setEnabled(false);
            set_params_status(params_status_, tr("Looking up %1…").arg(typed), ui::colors::TEXT_SECONDARY());
            symbol_resolve_id_ = services::InstrumentResolver::instance().resolve(
                typed, symbol_input_->options(), this,
                [this, typed, wf = current_workflow_type_](const services::ResolveResult& r) {
                    symbol_resolve_id_ = 0;
                    run_btn_->setEnabled(true);
                    if (r.exact && !r.matches.isEmpty() && services::is_quotable(r.matches.first())) {
                        symbol_input_->set_instrument(r.matches.first());
                        params_status_->clear();
                        if (wf == current_workflow_type_) // never start a run the user did not ask for
                            run_current_workflow();       // now with a picked instrument
                        return;
                    }
                    set_params_status(params_status_,
                                      r.matches.isEmpty()
                                          ? tr("No symbol matches \"%1\"").arg(typed)
                                          : tr("\"%1\" is more than one listing: pick one from the list").arg(typed),
                                      ui::colors::DANGER());
                    symbol_input_->focus_and_select();
                });
            return;
        }
    }

    executing_ = true;
    run_btn_->setEnabled(false);
    run_btn_->setText(tr("RUNNING..."));
    agent_result::show_plain(result_display_, QString());
    log_display_->clear();
    output_title_->setText(tr("OUTPUT  —  %1").arg(current_workflow_type_.toUpper()));
    output_status_->setText(tr("Executing..."));
    output_status_->setStyleSheet(QString("color:%1;font-size:" + fincept::ui::fonts::px(-4) + "px;").arg(ui::colors::AMBER()));
    log_display_->append(QString("[START] %1").arg(current_workflow_type_));

    QJsonObject params;
    if (symbol_row_->isVisible()) {
        const auto picked = symbol_input_->current_instrument();
        const QString sym = picked.is_valid() ? picked.symbol : QString();
        if (sym.isEmpty() && current_workflow_type_ != QLatin1String("options_scan")) {
            executing_ = false;
            run_btn_->setEnabled(true);
            run_btn_->setText(tr("RUN WORKFLOW"));
            set_params_status(params_status_, tr("Symbol is required"), ui::colors::DANGER());
            return;
        }
        if (!sym.isEmpty()) { // options_scan: empty = scan the whole market
            params["symbol"] = sym;
            log_display_->append(QString("[PARAM] symbol=%1").arg(sym));
        }
    }
    if (query_row_->isVisible()) {
        const QString q = query_input_->toPlainText().trimmed();
        if (q.isEmpty()) {
            executing_ = false;
            run_btn_->setEnabled(true);
            run_btn_->setText(tr("RUN WORKFLOW"));
            set_params_status(params_status_, tr("Query is required"), ui::colors::DANGER());
            return;
        }
        params["query"] = q;
    }

    if (portfolio_row_->isVisible() && !portfolio_combo_->currentData().toString().isEmpty()) {
        const QString pf_id = portfolio_combo_->currentData().toString();
        QJsonObject pd;
        pd["portfolio_id"] = pf_id;
        pd["name"] = portfolio_combo_->currentText();
        QJsonArray holdings;
        const auto assets = PortfolioRepository::instance().get_assets(pf_id);
        if (assets.is_ok()) {
            for (const auto& a : assets.value()) {
                QJsonObject h;
                h["symbol"] = a.symbol;
                h["quantity"] = a.quantity;
                h["avg_cost"] = a.avg_buy_price;
                holdings.append(h);
            }
        }
        pd["holdings"] = holdings;
        params["portfolio_data"] = pd;
        log_display_->append(QString("[PARAM] portfolio=%1 (%2 holdings)").arg(pd["name"].toString()).arg(holdings.size()));
    }

    // Always sent: empty means "inherit" (the workflow default shown under the
    // picker); AgentService::run_workflow resolves either into the run's LLM.
    params["llm_profile_id"] = llm_profile_combo_->currentData().toString();

    params_status_->clear();
    result_stack_->setCurrentWidget(result_display_);
    pending_request_id_ = services::AgentService::instance().run_workflow(current_workflow_type_, params);
    log_display_->append(QString("[REQ] %1").arg(pending_request_id_.left(8)));
}

void WorkflowsViewPanel::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    // Portfolios can be created/removed on the Portfolio screen between visits.
    if (portfolio_row_ && portfolio_row_->isVisible())
        refresh_portfolios();
}

void WorkflowsViewPanel::refresh_portfolios() {
    const QString prev_id = portfolio_combo_->currentData().toString();
    portfolio_combo_->blockSignals(true);
    portfolio_combo_->clear();
    portfolio_combo_->addItem(tr("None (let the agent look it up)"), QString{});
    const auto res = PortfolioRepository::instance().list_portfolios();
    if (res.is_ok()) {
        for (const auto& pf : res.value())
            portfolio_combo_->addItem(pf.name, pf.id);
    }
    const int restore = prev_id.isEmpty() ? 0 : portfolio_combo_->findData(prev_id);
    portfolio_combo_->setCurrentIndex(restore >= 0 ? restore : 0);
    portfolio_combo_->blockSignals(false);
}

// ── Re-translation ───────────────────────────────────────────────────────────

void WorkflowsViewPanel::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void WorkflowsViewPanel::retranslateUi() {
    // Catalog (the list items come from the kWorkflows table; rebuild them so a
    // live language switch re-translates the category headers + workflow labels).
    if (catalog_title_)
        catalog_title_->setText(tr("WORKFLOWS"));
    if (catalog_list_) {
        QString last_cat;
        catalog_list_->clear();
        for (int i = 0; i < kWorkflowCount; ++i) {
            const auto& wf = kWorkflows[i];
            if (QString(wf.category) != last_cat) {
                last_cat = wf.category;
                auto* sep = new QListWidgetItem(
                    QString("  %1").arg(QCoreApplication::translate("WorkflowsViewPanel", wf.category)));
                sep->setFlags(Qt::NoItemFlags);
                style_category_header(sep);
                catalog_list_->addItem(sep);
            }
            auto* item =
                new QListWidgetItem(QString("  %1").arg(QCoreApplication::translate("WorkflowsViewPanel", wf.label)));
            item->setData(Qt::UserRole, QString(wf.id));
            catalog_list_->addItem(item);
        }
    }

    // Params column.
    if (llm_profile_title_)
        llm_profile_title_->setText(tr("LLM PROFILE"));
    if (symbol_label_)
        symbol_label_->setText(tr("SYMBOL"));
    if (portfolio_label_)
        portfolio_label_->setText(tr("PORTFOLIO"));
    if (symbol_input_)
        symbol_input_->set_placeholder(tr("Ticker or company, e.g. AAPL"));
    if (query_label_)
        query_label_->setText(tr("QUERY"));
    if (query_input_)
        query_input_->setPlaceholderText(tr("Enter query for this workflow..."));
    if (run_btn_ && !executing_)
        run_btn_->setText(tr("RUN WORKFLOW"));

    // Output column section titles (status text holds live state — not re-applied).
    if (output_log_title_)
        output_log_title_->setText(tr("EXECUTION LOG"));
    if (output_result_title_)
        output_result_title_->setText(tr("RESULT"));

    // Dynamic-state labels: only re-apply the static default when idle, so a
    // language switch mid-run does not clobber the current selection / output.
    if (!executing_ && current_workflow_type_.isEmpty()) {
        if (params_title_)
            params_title_->setText(tr("PARAMETERS"));
        if (output_title_)
            output_title_->setText(tr("OUTPUT"));
        if (wf_desc_label_)
            wf_desc_label_->setText(tr("Select a workflow to configure and run."));
    }
}

// ── Draft persistence ────────────────────────────────────────────────────────

QVariantMap WorkflowsViewPanel::save_draft() const {
    QVariantMap d;
    if (symbol_input_ && !symbol_input_->text().isEmpty())
        d["symbol"] = symbol_input_->text();
    if (query_input_ && !query_input_->toPlainText().isEmpty())
        d["query"] = query_input_->toPlainText();
    // The markdown source (not the rendered plain text), so a restore re-renders it.
    if (result_display_ && !agent_result::saved_text(result_display_).isEmpty())
        d["result"] = agent_result::saved_text(result_display_);
    if (log_display_ && !log_display_->toPlainText().isEmpty())
        d["log"] = log_display_->toPlainText();
    return d;
}

void WorkflowsViewPanel::restore_draft(const QVariantMap& d) {
    if (d.isEmpty())
        return;
    if (symbol_input_ && d.contains("symbol"))
        symbol_input_->set_text(d["symbol"].toString()); // resolved again on RUN
    if (query_input_ && d.contains("query"))
        query_input_->setPlainText(d["query"].toString());
    if (result_display_ && d.contains("result")) {
        agent_result::show_markdown(result_display_, d["result"].toString());
        result_stack_->setCurrentWidget(result_display_); // the last result, not the preview
    }
    if (log_display_ && d.contains("log"))
        log_display_->setPlainText(d["log"].toString());
}

} // namespace fincept::screens
