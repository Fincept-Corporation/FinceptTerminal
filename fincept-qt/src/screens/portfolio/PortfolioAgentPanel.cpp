// src/screens/portfolio/PortfolioAgentPanel.cpp
#include "screens/portfolio/PortfolioAgentPanel.h"
#include "ai_chat/LlmService.h"
#include "core/logging/Logger.h"
#include "storage/repositories/AgentConfigRepository.h"
#include "ui/theme/Theme.h"

#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QRegularExpression>
#include <QVBoxLayout>
#include <QtConcurrent>

namespace fincept::screens {

PortfolioAgentPanel::PortfolioAgentPanel(QWidget* parent) : QWidget(parent) {
    setFixedWidth(480);
    build_ui();
    hide();
}

void PortfolioAgentPanel::build_ui() {
    setStyleSheet(QString(
        "background:%1; border:1px solid #00D4AA;")
        .arg(ui::colors::BG_BASE));

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // ── Header ───────────────────────────────────────────────────────────────
    auto* header = new QWidget;
    header->setFixedHeight(32);
    header->setStyleSheet("background:rgba(0,212,170,0.07); border-bottom:1px solid #2A2A2A;");

    auto* h_layout = new QHBoxLayout(header);
    h_layout->setContentsMargins(12, 0, 8, 0);
    h_layout->setSpacing(6);

    auto* icon = new QLabel("\U0001F916");
    icon->setStyleSheet("font-size:12px;");
    h_layout->addWidget(icon);

    auto* title = new QLabel("AGENT RUNNER");
    title->setStyleSheet("color:#00D4AA; font-size:10px; font-weight:800; letter-spacing:1px;");
    h_layout->addWidget(title);

    status_lbl_ = new QLabel;
    status_lbl_->setStyleSheet("color:#00D4AA; font-size:9px;");
    h_layout->addWidget(status_lbl_);

    h_layout->addStretch();

    close_btn_ = new QPushButton("\u2715");
    close_btn_->setFixedSize(18, 18);
    close_btn_->setCursor(Qt::PointingHandCursor);
    close_btn_->setStyleSheet(
        "QPushButton { background:none; border:none; color:#787878; font-size:11px; }"
        "QPushButton:hover { color:#e5e5e5; }");
    connect(close_btn_, &QPushButton::clicked, this, [this]() {
        hide();
        emit close_requested();
    });
    h_layout->addWidget(close_btn_);

    layout->addWidget(header);

    // ── Agent selector ───────────────────────────────────────────────────────
    auto* selector = new QWidget;
    selector->setStyleSheet(QString("background:%1; border-bottom:1px solid #1A1A1A;")
                            .arg(ui::colors::BG_SURFACE));
    auto* sel_layout = new QVBoxLayout(selector);
    sel_layout->setContentsMargins(12, 8, 12, 8);
    sel_layout->setSpacing(4);

    auto* sel_label = new QLabel("SELECT AGENT");
    sel_label->setStyleSheet("color:#555; font-size:8px; font-weight:700; letter-spacing:0.5px;");
    sel_layout->addWidget(sel_label);

    agent_cb_ = new QComboBox;
    agent_cb_->setFixedHeight(26);
    agent_cb_->setStyleSheet(QString(
        "QComboBox { background:%1; color:%2; border:1px solid #2A2A2A;"
        "  padding:0 8px; font-size:10px; }"
        "QComboBox::drop-down { border:none; }"
        "QComboBox QAbstractItemView { background:%1; color:%2; }")
        .arg(ui::colors::BG_BASE, ui::colors::TEXT_PRIMARY));
    connect(agent_cb_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        QString id = agent_cb_->currentData().toString();
        if (result_cache_.contains(id))
            content_->setHtml(result_cache_[id]);
        else
            content_->clear();
    });
    sel_layout->addWidget(agent_cb_);

    // Run button
    run_btn_ = new QPushButton("\U0001F916 RUN AGENT ON PORTFOLIO");
    run_btn_->setFixedHeight(26);
    run_btn_->setCursor(Qt::PointingHandCursor);
    run_btn_->setStyleSheet(
        "QPushButton { background:rgba(0,212,170,0.13); color:#00D4AA;"
        "  border:1px solid rgba(0,212,170,0.27); font-size:9px; font-weight:700;"
        "  letter-spacing:0.5px; margin-top:6px; }"
        "QPushButton:hover { background:rgba(0,212,170,0.22); }"
        "QPushButton:disabled { color:#555; }");
    connect(run_btn_, &QPushButton::clicked, this, [this]() { run_agent(true); });
    sel_layout->addWidget(run_btn_);

    layout->addWidget(selector);

    // ── Content ──────────────────────────────────────────────────────────────
    content_ = new QTextBrowser;
    content_->setOpenExternalLinks(true);
    content_->setStyleSheet(QString(
        "QTextBrowser { background:%1; color:%2; border:none; padding:12px;"
        "  font-size:11px; line-height:1.6; }")
        .arg(ui::colors::BG_BASE, ui::colors::TEXT_PRIMARY));
    content_->setPlaceholderText("Select an agent above and click Run.");
    layout->addWidget(content_, 1);
}

void PortfolioAgentPanel::set_summary(const portfolio::PortfolioSummary& summary) {
    if (summary.portfolio.id != last_portfolio_id_) {
        result_cache_.clear();
        content_->clear();
        last_portfolio_id_ = summary.portfolio.id;
    }
    summary_ = summary;
}

void PortfolioAgentPanel::show_panel() {
    reload_agents();
    show();
    raise();
}

void PortfolioAgentPanel::reload_agents() {
    agent_cb_->clear();
    auto r = AgentConfigRepository::instance().list_all();
    if (r.is_ok()) {
        for (const auto& a : r.value()) {
            QString label = a.name;
            if (!a.description.isEmpty())
                label += " \u2014 " + a.description.left(40);
            agent_cb_->addItem(label, a.id);
        }
    }
    if (agent_cb_->count() == 0) {
        agent_cb_->addItem("No agents saved \u2014 create one in Agent Config", "");
    }
}

void PortfolioAgentPanel::run_agent(bool force) {
    if (running_ || summary_.holdings.isEmpty()) return;

    QString agent_id = agent_cb_->currentData().toString();
    if (agent_id.isEmpty()) return;

    if (!force && result_cache_.contains(agent_id)) {
        content_->setHtml(result_cache_[agent_id]);
        return;
    }

    auto& llm = ai_chat::LlmService::instance();
    if (!llm.is_configured()) {
        content_->setPlainText("No LLM configured. Please add a model in Settings -> LLM Configuration.");
        return;
    }

    // Get agent config
    auto agent_r = AgentConfigRepository::instance().get(agent_id);
    if (agent_r.is_err()) {
        content_->setPlainText("Failed to load agent configuration.");
        return;
    }

    auto agent_cfg = agent_r.value();
    QString instructions = "You are a professional portfolio analyst. Analyze the provided portfolio data and give actionable insights.";

    // Parse config JSON for custom instructions
    auto cfg_doc = QJsonDocument::fromJson(agent_cfg.config_json.toUtf8());
    if (cfg_doc.isObject()) {
        auto cfg_obj = cfg_doc.object();
        if (cfg_obj.contains("instructions"))
            instructions = cfg_obj["instructions"].toString();
    }

    running_ = true;
    run_btn_->setEnabled(false);
    status_lbl_->setText("\u23F3");
    content_->setPlainText("Agent is analyzing your portfolio...");

    // Build portfolio context
    QStringList lines;
    lines << QString("Portfolio: %1 | NAV: %2 %3")
        .arg(summary_.portfolio.name, summary_.portfolio.currency)
        .arg(QString::number(summary_.total_market_value, 'f', 2));
    lines << QString("Total P&L: %1% | Positions: %2")
        .arg(QString::number(summary_.total_unrealized_pnl_percent, 'f', 2))
        .arg(summary_.total_positions);

    for (const auto& h : summary_.holdings) {
        lines << QString("%1: %2 units @ %3 | P&L: %4% | Weight: %5%")
            .arg(h.symbol)
            .arg(QString::number(h.quantity, 'f', 2))
            .arg(QString::number(h.current_price, 'f', 2))
            .arg(QString::number(h.unrealized_pnl_percent, 'f', 2))
            .arg(QString::number(h.weight, 'f', 1));
    }
    lines << "" << instructions;

    QString prompt = lines.join("\n");
    QPointer<PortfolioAgentPanel> self = this;

    QtConcurrent::run([self, prompt, agent_id]() {
        auto response = ai_chat::LlmService::instance().chat(prompt, {});

        if (!self) return;
        QMetaObject::invokeMethod(self, [self, response, agent_id]() {
            if (!self) return;
            self->running_ = false;
            self->run_btn_->setEnabled(true);
            self->status_lbl_->clear();

            if (response.success) {
                QString html = response.content;
                html.replace("&", "&amp;");
                html.replace("<", "&lt;");
                html.replace(">", "&gt;");
                html.replace("\n", "<br>");
                static const QRegularExpression bold_re("\\*\\*(.+?)\\*\\*");
                html.replace(bold_re, "<b>\\1</b>");
                html = QString("<div style='font-size:11px; line-height:1.6; color:#e5e5e5;'>%1</div>").arg(html);

                self->result_cache_[agent_id] = html;
                self->content_->setHtml(html);
                self->run_btn_->setText("\U0001F916 RE-RUN AGENT");
            } else {
                self->content_->setPlainText("Agent run failed: " + response.error);
            }
        }, Qt::QueuedConnection);
    });
}

} // namespace fincept::screens
