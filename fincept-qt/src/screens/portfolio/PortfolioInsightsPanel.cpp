// src/screens/portfolio/PortfolioInsightsPanel.cpp
#include "screens/portfolio/PortfolioInsightsPanel.h"

#include "ai_chat/LlmService.h"
#include "core/logging/Logger.h"
#include "services/agents/AgentService.h"
#include "ui/markdown/MarkdownRenderer.h"
#include "ui/theme/Theme.h"

#include <QComboBox>
#include <QDateTime>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLabel>
#include <QPointer>
#include <QPushButton>
#include <QScrollBar>
#include <QShowEvent>
#include <QStackedWidget>
#include <QTextBrowser>
#include <QVBoxLayout>

namespace fincept::screens {

namespace {

QString fmt_now() {
    return QDateTime::currentDateTime().toString("HH:mm:ss");
}

// Markdown rendering is delegated to ui::MarkdownRenderer — the same path used
// by AgentChatPanel and AI Chat so portfolio responses look consistent.

} // namespace

PortfolioInsightsPanel::PortfolioInsightsPanel(QWidget* parent) : QWidget(parent) {
    setObjectName("PortfolioInsightsPanel");
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_StyledBackground, true);
    setFixedWidth(560);
    build_ui();
    hide();

    // Wire AgentService results. Both run_portfolio_analysis (AI tab) and
    // run_agent_streaming (Agent tab) come back through this service — we
    // demultiplex by the pending type / request id we stored when we kicked
    // off the run.
    auto& svc = services::AgentService::instance();

    connect(&svc, &services::AgentService::agent_result, this, [this](services::AgentExecutionResult r) {
        // Only the AI tab uses the non-streaming run_portfolio_analysis path.
        if (ai_pending_type_.isEmpty())
            return;
        const QString type = ai_pending_type_;
        ai_pending_type_.clear();
        ai_busy_ = false;
        ai_run_->setEnabled(true);
        header_status_->clear();
        if (r.success && !r.response.isEmpty()) {
            ai_cache_.insert(type, r.response);
            ai_meta_->setText(QString("Last run %1  •  %2ms").arg(fmt_now()).arg(r.execution_time_ms));
            render_result(ai_content_, r.response);
            QString upper = type.toUpper();
            if (upper == "OPPORTUNITIES")
                upper = "OPPS";
            ai_run_->setText(QString("RE-RUN %1 ANALYSIS").arg(upper));
        } else {
            const QString msg = r.error.isEmpty() ? QStringLiteral("No response received.") : r.error;
            render_error(ai_content_, "Analysis failed.\n\n" + msg);
        }
    });

    connect(&svc, &services::AgentService::agent_stream_thinking, this,
            [this](const QString& request_id, const QString& status) {
                if (request_id != agent_pending_req_id_)
                    return;
                header_status_->setText("● " + status);
            });

    // Live token streaming — accumulate into agent_streaming_text_ and show
    // the partial response as plain text so the user sees progress. Final
    // markdown rendering happens on agent_stream_done.
    connect(&svc, &services::AgentService::agent_stream_token, this,
            [this](const QString& request_id, const QString& token) {
                if (request_id != agent_pending_req_id_)
                    return;
                agent_streaming_text_ += token;
                if (agent_content_) {
                    agent_content_->setPlainText(agent_streaming_text_);
                    auto* sb = agent_content_->verticalScrollBar();
                    sb->setValue(sb->maximum());
                }
            });

    connect(&svc, &services::AgentService::agent_stream_done, this,
            [this](services::AgentExecutionResult r) {
                if (r.request_id != agent_pending_req_id_)
                    return;
                const QString agent_id = agent_pending_id_;
                agent_pending_req_id_.clear();
                agent_pending_id_.clear();
                agent_busy_ = false;
                agent_run_->setEnabled(true);
                header_status_->clear();

                // finagent_core emits some agents via the final JSON line
                // (r.response populated) and others via streamed tokens (where
                // r.response may come back empty). Mirror AgentChatPanel's
                // logic: prefer r.response, fall back to accumulated tokens.
                QString final_text = r.response.trimmed();
                if (final_text.isEmpty())
                    final_text = agent_streaming_text_.trimmed();
                agent_streaming_text_.clear();

                if (r.success && !final_text.isEmpty()) {
                    agent_cache_.insert(agent_id, final_text);
                    agent_meta_->setText(QString("Last run %1  •  %2ms").arg(fmt_now()).arg(r.execution_time_ms));
                    render_result(agent_content_, final_text);
                    agent_run_->setText("RE-RUN AGENT");
                } else if (r.success) {
                    // Agent reported success but produced no text — likely a
                    // config issue (no LLM key in the agent's profile, etc.).
                    render_error(agent_content_,
                                 "Agent completed but returned no content.\n\n"
                                 "Check the agent's LLM profile in Agent Config → Agents, "
                                 "and make sure an API key is set in Settings → LLM Configuration.");
                } else {
                    const QString msg = r.error.isEmpty() ? QStringLiteral("No response received.") : r.error;
                    render_error(agent_content_, "Agent run failed.\n\n" + msg);
                }
            });

    // Refresh the agent dropdown whenever finagent_core discovers agents.
    // This matches the AgentChatPanel wiring so both lists stay in sync.
    connect(&svc, &services::AgentService::agents_discovered, this,
            [this](QVector<services::AgentInfo>, QVector<services::AgentCategory>) {
                if (current_tab_ == Tab::Agent)
                    reload_agents();
            });

    connect(&svc, &services::AgentService::error_occurred, this,
            [this](const QString& context, const QString& message) {
                if (context != "run_portfolio_analysis" && context != "run_agent_streaming")
                    return;
                ai_busy_ = agent_busy_ = false;
                ai_run_->setEnabled(true);
                agent_run_->setEnabled(true);
                header_status_->clear();
                QTextBrowser* target = current_tab_ == Tab::AI ? ai_content_ : agent_content_;
                render_error(target, QString("Error (%1).\n\n%2").arg(context, message));
            });

    // If the user edits LLM config while the panel is open, refresh the
    // is_configured() check on the next run — nothing to do here except
    // keep a hook for future UI (status badge etc.).
    connect(&ai_chat::LlmService::instance(), &ai_chat::LlmService::config_changed, this,
            []() { LOG_INFO("InsightsPanel", "LLM config changed — next run will pick up new settings"); });
}

void PortfolioInsightsPanel::build_ui() {
    const QString bg = ui::colors::BG_BASE();
    const QString surface = ui::colors::BG_SURFACE();
    const QString hover = ui::colors::BG_HOVER();
    const QString border = ui::colors::BORDER_MED();
    const QString border_dim = ui::colors::BORDER_DIM();
    const QString amber = ui::colors::AMBER();
    const QString text1 = ui::colors::TEXT_PRIMARY();
    const QString text2 = ui::colors::TEXT_SECONDARY();
    const QString text3 = ui::colors::TEXT_TERTIARY();

    // Opaque background — this is the whole fix for the previous overlap bug.
    setStyleSheet(QString("#PortfolioInsightsPanel { background:%1; border-left:1px solid %2; }")
                      .arg(bg, border));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── Header ────────────────────────────────────────────────────────────────
    auto* header = new QWidget(this);
    header->setFixedHeight(44);
    header->setStyleSheet(QString("background:%1; border-bottom:1px solid %2;").arg(surface, border));
    auto* hl = new QHBoxLayout(header);
    hl->setContentsMargins(16, 0, 8, 0);
    hl->setSpacing(8);

    auto* title = new QLabel("PORTFOLIO INSIGHTS");
    title->setStyleSheet(QString("color:%1; font-size:11px; font-weight:800; letter-spacing:2px;").arg(amber));
    hl->addWidget(title);

    header_status_ = new QLabel;
    header_status_->setStyleSheet(QString("color:%1; font-size:10px;").arg(text2));
    hl->addWidget(header_status_);
    hl->addStretch();

    auto* close_btn = new QPushButton("×");
    close_btn->setFixedSize(28, 28);
    close_btn->setCursor(Qt::PointingHandCursor);
    close_btn->setToolTip("Close  (Esc)");
    close_btn->setStyleSheet(QString("QPushButton { background:transparent; border:none; color:%1;"
                                     "  font-size:20px; font-weight:300; }"
                                     "QPushButton:hover { color:%2; background:%3; }")
                                 .arg(text2, text1, hover));
    connect(close_btn, &QPushButton::clicked, this, [this]() { hide(); emit close_requested(); });
    hl->addWidget(close_btn);
    root->addWidget(header);

    // ── Tab bar ───────────────────────────────────────────────────────────────
    auto* tab_bar = new QWidget(this);
    tab_bar->setFixedHeight(36);
    tab_bar->setStyleSheet(QString("background:%1; border-bottom:1px solid %2;").arg(bg, border_dim));
    auto* tl = new QHBoxLayout(tab_bar);
    tl->setContentsMargins(0, 0, 0, 0);
    tl->setSpacing(0);

    auto make_tab = [&](const QString& txt) {
        auto* b = new QPushButton(txt);
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        b->setStyleSheet(QString("QPushButton { background:transparent; color:%1; border:none;"
                                 "  border-bottom:2px solid transparent; padding:8px 0;"
                                 "  font-size:10px; font-weight:700; letter-spacing:1.5px; }"
                                 "QPushButton:checked { color:%2; border-bottom:2px solid %2;"
                                 "  background:rgba(217,119,6,0.06); }"
                                 "QPushButton:hover:!checked { color:%3; }")
                             .arg(text2, amber, text1));
        return b;
    };
    tab_ai_btn_ = make_tab("AI ANALYSIS");
    tab_agent_btn_ = make_tab("AGENT RUNNER");
    tab_ai_btn_->setChecked(true);
    connect(tab_ai_btn_, &QPushButton::clicked, this, [this]() { switch_tab(Tab::AI); });
    connect(tab_agent_btn_, &QPushButton::clicked, this, [this]() { switch_tab(Tab::Agent); });
    tl->addWidget(tab_ai_btn_, 1);
    tl->addWidget(tab_agent_btn_, 1);
    root->addWidget(tab_bar);

    // ── Pages ─────────────────────────────────────────────────────────────────
    pages_ = new QStackedWidget(this);
    pages_->addWidget(build_ai_page());
    pages_->addWidget(build_agent_page());
    root->addWidget(pages_, 1);
}

QWidget* PortfolioInsightsPanel::build_ai_page() {
    const QString bg = ui::colors::BG_BASE();
    const QString surface = ui::colors::BG_SURFACE();
    const QString border_dim = ui::colors::BORDER_DIM();
    const QString amber = ui::colors::AMBER();
    const QString text1 = ui::colors::TEXT_PRIMARY();
    const QString text2 = ui::colors::TEXT_SECONDARY();
    const QString text3 = ui::colors::TEXT_TERTIARY();

    auto* page = new QWidget;
    page->setStyleSheet(QString("background:%1;").arg(bg));
    auto* lay = new QVBoxLayout(page);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);

    // Config block
    auto* cfg = new QWidget(page);
    cfg->setStyleSheet(QString("background:%1; border-bottom:1px solid %2;").arg(surface, border_dim));
    auto* cl = new QVBoxLayout(cfg);
    cl->setContentsMargins(16, 12, 16, 12);
    cl->setSpacing(10);

    auto* label = new QLabel("ANALYSIS TYPE");
    label->setStyleSheet(QString("color:%1; font-size:9px; font-weight:700; letter-spacing:1.5px;").arg(text3));
    cl->addWidget(label);

    auto* pill_row = new QHBoxLayout;
    pill_row->setSpacing(6);
    auto make_pill = [&](const QString& txt, const QString& id) {
        auto* p = new QPushButton(txt);
        p->setCheckable(true);
        p->setCursor(Qt::PointingHandCursor);
        p->setFixedHeight(28);
        p->setStyleSheet(QString("QPushButton { background:%1; color:%2; border:1px solid %3;"
                                 "  padding:0 14px; font-size:10px; font-weight:700; letter-spacing:1px; }"
                                 "QPushButton:checked { background:rgba(217,119,6,0.15); color:%4;"
                                 "  border:1px solid %4; }"
                                 "QPushButton:hover:!checked { color:%5; border:1px solid %5; }")
                             .arg(bg, text2, border_dim, amber, text1));
        connect(p, &QPushButton::clicked, this, [this, id]() { set_ai_type(id); });
        pill_row->addWidget(p);
        return p;
    };
    ai_full_ = make_pill("FULL", "full");
    ai_risk_ = make_pill("RISK", "risk");
    ai_rebal_ = make_pill("REBALANCE", "rebalance");
    ai_opport_ = make_pill("OPPS", "opportunities");
    ai_full_->setChecked(true);
    pill_row->addStretch();
    cl->addLayout(pill_row);

    ai_run_ = new QPushButton("RUN FULL ANALYSIS");
    ai_run_->setCursor(Qt::PointingHandCursor);
    ai_run_->setFixedHeight(36);
    ai_run_->setStyleSheet(QString("QPushButton { background:%1; color:%2; border:none;"
                                   "  font-size:11px; font-weight:700; letter-spacing:2px; }"
                                   "QPushButton:hover { background:%3; }"
                                   "QPushButton:disabled { background:%4; color:%5; }")
                               .arg(amber, bg, ui::colors::AMBER_DIM(), border_dim, text3));
    connect(ai_run_, &QPushButton::clicked, this, [this]() { run_ai(true); });
    cl->addWidget(ai_run_);

    ai_meta_ = new QLabel;
    ai_meta_->setStyleSheet(QString("color:%1; font-size:9px;").arg(text3));
    cl->addWidget(ai_meta_);

    lay->addWidget(cfg);

    // Results
    ai_content_ = new QTextBrowser(page);
    ai_content_->setOpenExternalLinks(true);
    ai_content_->setStyleSheet(QString("QTextBrowser { background:%1; color:%2; border:none; padding:16px;"
                                       "  font-size:12px; }")
                                   .arg(bg, text1));
    lay->addWidget(ai_content_, 1);
    render_empty(ai_content_, "Select an analysis type and press RUN.\n\n"
                             "FULL — broad review of holdings and allocation.\n"
                             "RISK — concentration, correlation, downside scenarios.\n"
                             "REBALANCE — target weights with rationale.\n"
                             "OPPS — undervalued positions and gaps.");
    return page;
}

QWidget* PortfolioInsightsPanel::build_agent_page() {
    const QString bg = ui::colors::BG_BASE();
    const QString surface = ui::colors::BG_SURFACE();
    const QString border_dim = ui::colors::BORDER_DIM();
    const QString amber = ui::colors::AMBER();
    const QString text1 = ui::colors::TEXT_PRIMARY();
    const QString text2 = ui::colors::TEXT_SECONDARY();
    const QString text3 = ui::colors::TEXT_TERTIARY();

    auto* page = new QWidget;
    page->setStyleSheet(QString("background:%1;").arg(bg));
    auto* lay = new QVBoxLayout(page);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);

    auto* cfg = new QWidget(page);
    cfg->setStyleSheet(QString("background:%1; border-bottom:1px solid %2;").arg(surface, border_dim));
    auto* cl = new QVBoxLayout(cfg);
    cl->setContentsMargins(16, 12, 16, 12);
    cl->setSpacing(8);

    auto* label = new QLabel("SELECT AGENT");
    label->setStyleSheet(QString("color:%1; font-size:9px; font-weight:700; letter-spacing:1.5px;").arg(text3));
    cl->addWidget(label);

    agent_cb_ = new QComboBox;
    agent_cb_->setFixedHeight(30);
    agent_cb_->setStyleSheet(QString("QComboBox { background:%1; color:%2; border:1px solid %3;"
                                     "  padding:0 10px; font-size:11px; }"
                                     "QComboBox::drop-down { border:none; width:20px; }"
                                     "QComboBox QAbstractItemView { background:%1; color:%2; border:1px solid %3;"
                                     "  selection-background-color:rgba(217,119,6,0.15); }")
                                 .arg(bg, text1, border_dim));
    connect(agent_cb_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        if (!agent_cb_ || !agent_desc_ || !agent_content_)
            return;
        agent_desc_->setText(agent_cb_->currentData(Qt::UserRole + 1).toString());
        QString id = agent_cb_->currentData().toString();
        if (agent_cache_.contains(id))
            render_result(agent_content_, agent_cache_.value(id));
        else
            render_empty(agent_content_, "Press RUN to analyze your portfolio with this agent.");
    });
    cl->addWidget(agent_cb_);

    agent_desc_ = new QLabel;
    agent_desc_->setWordWrap(true);
    agent_desc_->setStyleSheet(QString("color:%1; font-size:10px; line-height:1.5;").arg(text2));
    cl->addWidget(agent_desc_);

    agent_run_ = new QPushButton("RUN AGENT");
    agent_run_->setCursor(Qt::PointingHandCursor);
    agent_run_->setFixedHeight(36);
    agent_run_->setStyleSheet(QString("QPushButton { background:%1; color:%2; border:none;"
                                      "  font-size:11px; font-weight:700; letter-spacing:2px; }"
                                      "QPushButton:hover { background:%3; }"
                                      "QPushButton:disabled { background:%4; color:%5; }")
                                  .arg(amber, bg, ui::colors::AMBER_DIM(), border_dim, text3));
    connect(agent_run_, &QPushButton::clicked, this, [this]() { run_agent(true); });
    cl->addWidget(agent_run_);

    agent_meta_ = new QLabel;
    agent_meta_->setStyleSheet(QString("color:%1; font-size:9px;").arg(text3));
    cl->addWidget(agent_meta_);

    lay->addWidget(cfg);

    agent_content_ = new QTextBrowser(page);
    agent_content_->setOpenExternalLinks(true);
    agent_content_->setStyleSheet(QString("QTextBrowser { background:%1; color:%2; border:none; padding:16px;"
                                          "  font-size:12px; }")
                                      .arg(bg, text1));
    lay->addWidget(agent_content_, 1);
    render_empty(agent_content_, "Load a saved agent above and press RUN.");
    return page;
}

void PortfolioInsightsPanel::set_summary(const portfolio::PortfolioSummary& summary) {
    if (summary.portfolio.id != last_portfolio_id_) {
        ai_cache_.clear();
        agent_cache_.clear();
        if (ai_content_)
            render_empty(ai_content_, "Select an analysis type and press RUN.");
        if (agent_content_)
            render_empty(agent_content_, "Load a saved agent above and press RUN.");
        last_portfolio_id_ = summary.portfolio.id;
    }
    summary_ = summary;
}

void PortfolioInsightsPanel::open_tab(Tab tab) {
    switch_tab(tab);
    show();
    raise();
    setFocus();
}

void PortfolioInsightsPanel::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    if (current_tab_ == Tab::Agent)
        reload_agents();
}

void PortfolioInsightsPanel::keyPressEvent(QKeyEvent* e) {
    if (e->key() == Qt::Key_Escape) {
        hide();
        emit close_requested();
        return;
    }
    QWidget::keyPressEvent(e);
}

void PortfolioInsightsPanel::switch_tab(Tab tab) {
    current_tab_ = tab;
    tab_ai_btn_->setChecked(tab == Tab::AI);
    tab_agent_btn_->setChecked(tab == Tab::Agent);
    pages_->setCurrentIndex(tab == Tab::AI ? 0 : 1);
    if (tab == Tab::Agent)
        reload_agents();
}

void PortfolioInsightsPanel::set_ai_type(const QString& type) {
    ai_type_ = type;
    ai_full_->setChecked(type == "full");
    ai_risk_->setChecked(type == "risk");
    ai_rebal_->setChecked(type == "rebalance");
    ai_opport_->setChecked(type == "opportunities");

    QString upper = type.toUpper();
    if (upper == "OPPORTUNITIES")
        upper = "OPPS";
    ai_run_->setText(ai_cache_.contains(type) ? QString("RE-RUN %1 ANALYSIS").arg(upper)
                                              : QString("RUN %1 ANALYSIS").arg(upper));

    if (ai_cache_.contains(type))
        render_result(ai_content_, ai_cache_.value(type));
    else
        render_empty(ai_content_, "Press RUN to start this analysis.");
}

void PortfolioInsightsPanel::reload_agents() {
    if (!agent_cb_)
        return;
    const QString prev = agent_cb_->currentData().toString();
    agent_cb_->blockSignals(true);
    agent_cb_->clear();

    // Single source of truth: finagent_core agents from AgentService. We do
    // NOT fall back to AgentConfigRepository — that table only contains
    // user-saved overrides (often 0 or 2 rows) which would mislead the user
    // into thinking only those agents are available.
    const auto cached = services::AgentService::instance().cached_agents();
    if (!cached.isEmpty()) {
        for (const auto& a : cached) {
            const QString label = a.category.isEmpty()
                                      ? a.name
                                      : QString("[%1] %2").arg(a.category, a.name);
            agent_cb_->addItem(label, a.id);
            agent_cb_->setItemData(agent_cb_->count() - 1, a.description, Qt::UserRole + 1);
        }
    } else {
        // Cache cold — show a disabled "discovering" placeholder and kick
        // off discovery. agents_discovered will repopulate us when it lands.
        agent_cb_->addItem("Discovering agents…", "");
        agent_cb_->setItemData(0, "Loading agents from finagent_core. If this persists, make sure "
                                  "Python is installed and open Agent Config for more details.",
                               Qt::UserRole + 1);
        services::AgentService::instance().discover_agents();
    }

    int idx = prev.isEmpty() ? 0 : agent_cb_->findData(prev);
    agent_cb_->setCurrentIndex(idx < 0 ? 0 : idx);
    agent_cb_->blockSignals(false);
    if (agent_desc_)
        agent_desc_->setText(agent_cb_->currentData(Qt::UserRole + 1).toString());
}

QString PortfolioInsightsPanel::build_portfolio_context() const {
    QStringList lines;
    lines << QString("Portfolio: %1  |  NAV: %2 %3")
                 .arg(summary_.portfolio.name, summary_.portfolio.currency)
                 .arg(QString::number(summary_.total_market_value, 'f', 2));
    lines << QString("Total P&L: %1%  |  Day Change: %2%  |  Positions: %3")
                 .arg(QString::number(summary_.total_unrealized_pnl_percent, 'f', 2))
                 .arg(QString::number(summary_.total_day_change_percent, 'f', 2))
                 .arg(summary_.total_positions);
    lines << "";
    lines << "Holdings:";
    for (const auto& h : summary_.holdings) {
        lines << QString("  %1 [%2]  qty %3 @ %4  |  P&L %5%  |  wt %6%")
                     .arg(h.symbol, h.sector.isEmpty() ? "Unclassified" : h.sector)
                     .arg(QString::number(h.quantity, 'f', 2))
                     .arg(QString::number(h.current_price, 'f', 2))
                     .arg(QString::number(h.unrealized_pnl_percent, 'f', 2))
                     .arg(QString::number(h.weight, 'f', 1));
    }
    return lines.join("\n");
}

void PortfolioInsightsPanel::run_ai(bool force) {
    if (ai_busy_ || summary_.holdings.isEmpty())
        return;
    if (!force && ai_cache_.contains(ai_type_)) {
        render_result(ai_content_, ai_cache_.value(ai_type_));
        return;
    }
    auto& llm = ai_chat::LlmService::instance();
    if (!llm.is_configured()) {
        render_error(ai_content_,
                     "No LLM configured.\n\nOpen Settings → LLM Configuration, add a provider with an API key, "
                     "then try again. The Agent Config tab shares the same LLM setup.");
        return;
    }

    // Serialize the portfolio summary so the Python side can format it however
    // the analysis_type requires. Keys mirror what finagent_core expects.
    QJsonArray holdings;
    for (const auto& h : summary_.holdings) {
        QJsonObject o;
        o["symbol"] = h.symbol;
        o["sector"] = h.sector.isEmpty() ? QStringLiteral("Unclassified") : h.sector;
        o["quantity"] = h.quantity;
        o["avg_buy_price"] = h.avg_buy_price;
        o["current_price"] = h.current_price;
        o["market_value"] = h.market_value;
        o["cost_basis"] = h.cost_basis;
        o["unrealized_pnl"] = h.unrealized_pnl;
        o["unrealized_pnl_percent"] = h.unrealized_pnl_percent;
        o["weight"] = h.weight;
        holdings.append(o);
    }
    QJsonObject summary_json;
    summary_json["name"] = summary_.portfolio.name;
    summary_json["currency"] = summary_.portfolio.currency;
    summary_json["total_market_value"] = summary_.total_market_value;
    summary_json["total_cost_basis"] = summary_.total_cost_basis;
    summary_json["total_unrealized_pnl"] = summary_.total_unrealized_pnl;
    summary_json["total_unrealized_pnl_percent"] = summary_.total_unrealized_pnl_percent;
    summary_json["total_day_change_percent"] = summary_.total_day_change_percent;
    summary_json["total_positions"] = summary_.total_positions;
    summary_json["holdings"] = holdings;
    // Include a plain-text fallback for Python paths that ignore the structured
    // summary and just forward the prompt verbatim to the LLM.
    summary_json["context_text"] = build_portfolio_context();

    ai_busy_ = true;
    ai_pending_type_ = ai_type_;
    ai_run_->setEnabled(false);
    header_status_->setText(QString("● running %1…").arg(ai_type_));
    render_empty(ai_content_, QString("Running %1 analysis through the agent stack…").arg(ai_type_));

    services::AgentService::instance().run_portfolio_analysis(ai_type_, summary_json);
}

void PortfolioInsightsPanel::run_agent(bool force) {
    if (agent_busy_ || summary_.holdings.isEmpty())
        return;
    const QString agent_id = agent_cb_->currentData().toString();
    if (agent_id.isEmpty()) {
        render_error(agent_content_, "No agent selected.\n\nCreate one in the Agent Config screen, then return here.");
        return;
    }
    if (!force && agent_cache_.contains(agent_id)) {
        render_result(agent_content_, agent_cache_.value(agent_id));
        return;
    }
    auto& llm = ai_chat::LlmService::instance();
    if (!llm.is_configured()) {
        render_error(agent_content_,
                     "No LLM configured.\n\nOpen Settings → LLM Configuration, add a provider with an API key, "
                     "then try again.");
        return;
    }

    // Resolve the friendly agent name from the cached finagent list (the
    // canonical source). Fall back to the dropdown label if the cache was
    // cleared between populate and click.
    QString agent_name;
    for (const auto& a : services::AgentService::instance().cached_agents()) {
        if (a.id == agent_id) {
            agent_name = a.name;
            break;
        }
    }
    if (agent_name.isEmpty())
        agent_name = agent_cb_->currentText();

    // Compose the query that finagent_core will run through the configured
    // agent: portfolio context first, then a task prompt. The agent's own
    // system prompt (configured in Agent Config) shapes the style.
    const QString query = build_portfolio_context() +
                          "\n\nTask: Analyze this portfolio as " + agent_name +
                          ". Provide clear sections, concrete recommendations, and quantify exposures "
                          "where data allows.";

    QJsonObject config;
    config["agent_id"] = agent_id;

    agent_busy_ = true;
    agent_streaming_text_.clear();
    agent_run_->setEnabled(false);
    header_status_->setText(QString("● running %1…").arg(agent_name));
    render_empty(agent_content_, QString("Running %1 on this portfolio…").arg(agent_name));

    agent_pending_id_ = agent_id;
    agent_pending_req_id_ = services::AgentService::instance().run_agent_streaming(query, config);
}

void PortfolioInsightsPanel::render_result(QTextBrowser* target, const QString& markdown) {
    if (!target)
        return;
    if (markdown.trimmed().isEmpty()) {
        render_empty(target, "(empty response)");
        return;
    }
    target->setHtml(ui::MarkdownRenderer::render(markdown));
    target->verticalScrollBar()->setValue(0);
}

void PortfolioInsightsPanel::render_error(QTextBrowser* target, const QString& message) {
    if (!target)
        return;
    QString safe = message;
    safe.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;").replace("\n", "<br>");
    target->setHtml(QString("<div style='color:%1; font-size:12px; line-height:1.55;'>"
                            "<div style='color:%2; font-weight:700; letter-spacing:1px; margin-bottom:8px;'>"
                            "⚠ ERROR</div>%3</div>")
                        .arg(ui::colors::TEXT_PRIMARY(), ui::colors::NEGATIVE(), safe));
}

void PortfolioInsightsPanel::render_empty(QTextBrowser* target, const QString& hint) {
    if (!target)
        return;
    QString safe = hint;
    safe.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;").replace("\n", "<br>");
    target->setHtml(QString("<div style='color:%1; font-size:11px; line-height:1.7; padding-top:8px;'>%2</div>")
                        .arg(ui::colors::TEXT_TERTIARY(), safe));
}

} // namespace fincept::screens
