// src/screens/portfolio/PortfolioAiPanel.cpp
#include "screens/portfolio/PortfolioAiPanel.h"

#include "ai_chat/LlmService.h"
#include "core/logging/Logger.h"
#include "ui/theme/Theme.h"

#include <QHBoxLayout>
#include <QPointer>
#include <QRegularExpression>
#include <QVBoxLayout>
#include <QtConcurrent>

namespace fincept::screens {

PortfolioAiPanel::PortfolioAiPanel(QWidget* parent) : QWidget(parent) {
    setFixedWidth(480);
    build_ui();
    hide();
}

void PortfolioAiPanel::build_ui() {
    setStyleSheet(QString("background:%1; border:1px solid #9D4EDD;").arg(ui::colors::BG_BASE));

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // ── Header ───────────────────────────────────────────────────────────────
    auto* header = new QWidget(this);
    header->setFixedHeight(32);
    header->setStyleSheet(
        QString("background:rgba(157,78,221,0.09); border-bottom:1px solid %1;").arg(ui::colors::BORDER_MED));

    auto* h_layout = new QHBoxLayout(header);
    h_layout->setContentsMargins(12, 0, 8, 0);
    h_layout->setSpacing(6);

    auto* icon = new QLabel("\U0001F916");
    icon->setStyleSheet("font-size:12px;");
    h_layout->addWidget(icon);

    auto* title = new QLabel("AI PORTFOLIO ANALYSIS");
    title->setStyleSheet("color:#9D4EDD; font-size:10px; font-weight:800; letter-spacing:1px;");
    h_layout->addWidget(title);

    status_lbl_ = new QLabel;
    status_lbl_->setStyleSheet("color:#9D4EDD; font-size:9px;");
    h_layout->addWidget(status_lbl_);

    h_layout->addStretch();

    close_btn_ = new QPushButton("\u2715");
    close_btn_->setFixedSize(18, 18);
    close_btn_->setCursor(Qt::PointingHandCursor);
    close_btn_->setStyleSheet(QString("QPushButton { background:none; border:none; color:%1; font-size:11px; }"
                                      "QPushButton:hover { color:%2; }")
                                  .arg(ui::colors::TEXT_SECONDARY, ui::colors::TEXT_PRIMARY));
    connect(close_btn_, &QPushButton::clicked, this, [this]() {
        hide();
        emit close_requested();
    });
    h_layout->addWidget(close_btn_);

    layout->addWidget(header);

    // ── Analysis type selector ───────────────────────────────────────────────
    auto* type_bar = new QWidget(this);
    type_bar->setFixedHeight(28);
    type_bar->setStyleSheet(
        QString("background:%1; border-bottom:1px solid %2;").arg(ui::colors::BG_SURFACE, ui::colors::BORDER_MED));

    auto* type_layout = new QHBoxLayout(type_bar);
    type_layout->setContentsMargins(0, 0, 0, 0);
    type_layout->setSpacing(0);

    auto make_type_btn = [&](const QString& text, const QString& type_id) {
        auto* btn = new QPushButton(text);
        btn->setCheckable(true);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(QString("QPushButton { background:transparent; color:%1; border:none;"
                                   "  border-bottom:2px solid transparent; padding:4px 8px;"
                                   "  font-size:8px; font-weight:700; letter-spacing:0.5px; }"
                                   "QPushButton:checked { color:#9D4EDD; border-bottom:2px solid #9D4EDD;"
                                   "  background:rgba(157,78,221,0.08); }"
                                   "QPushButton:hover { color:#9D4EDD; }")
                               .arg(ui::colors::TEXT_SECONDARY));
        connect(btn, &QPushButton::clicked, this, [this, type_id]() { set_analysis_type(type_id); });
        type_layout->addWidget(btn, 1);
        return btn;
    };

    full_btn_ = make_type_btn("FULL", "full");
    risk_btn_ = make_type_btn("RISK", "risk");
    rebal_btn_ = make_type_btn("REBALANCE", "rebalance");
    opport_btn_ = make_type_btn("OPPORTUNITIES", "opportunities");
    full_btn_->setChecked(true);

    layout->addWidget(type_bar);

    // ── Run button ───────────────────────────────────────────────────────────
    run_btn_ = new QPushButton("\U0001F916 RUN FULL ANALYSIS");
    run_btn_->setFixedHeight(28);
    run_btn_->setCursor(Qt::PointingHandCursor);
    run_btn_->setStyleSheet(QString("QPushButton { background:rgba(157,78,221,0.13); color:#9D4EDD;"
                                    "  border:1px solid rgba(157,78,221,0.27); font-size:9px; font-weight:700;"
                                    "  letter-spacing:0.5px; margin:6px 12px; }"
                                    "QPushButton:hover { background:rgba(157,78,221,0.22); }"
                                    "QPushButton:disabled { color:%1; }")
                                .arg(ui::colors::TEXT_TERTIARY));
    connect(run_btn_, &QPushButton::clicked, this, [this]() { run_analysis(true); });
    layout->addWidget(run_btn_);

    // ── Content ──────────────────────────────────────────────────────────────
    content_ = new QTextBrowser;
    content_->setOpenExternalLinks(true);
    content_->setStyleSheet(QString("QTextBrowser { background:%1; color:%2; border:none; padding:12px;"
                                    "  font-size:11px; line-height:1.6; }")
                                .arg(ui::colors::BG_BASE, ui::colors::TEXT_PRIMARY));
    content_->setPlaceholderText("Select an analysis type above and click Run.");
    layout->addWidget(content_, 1);
}

void PortfolioAiPanel::set_summary(const portfolio::PortfolioSummary& summary) {
    // Clear cache if portfolio changed
    if (summary.portfolio.id != last_portfolio_id_) {
        result_cache_.clear();
        content_->clear();
        last_portfolio_id_ = summary.portfolio.id;
    }
    summary_ = summary;
}

void PortfolioAiPanel::show_panel() {
    show();
    raise();
}

void PortfolioAiPanel::set_analysis_type(const QString& type) {
    current_type_ = type;
    full_btn_->setChecked(type == "full");
    risk_btn_->setChecked(type == "risk");
    rebal_btn_->setChecked(type == "rebalance");
    opport_btn_->setChecked(type == "opportunities");

    run_btn_->setText(
        QString("\U0001F916 %1 %2 ANALYSIS").arg(result_cache_.contains(type) ? "RE-RUN" : "RUN").arg(type.toUpper()));

    // Show cached result if available
    if (result_cache_.contains(type)) {
        content_->setHtml(result_cache_[type]);
    } else {
        content_->clear();
    }
}

QString PortfolioAiPanel::build_portfolio_prompt() const {
    QStringList lines;
    lines << QString("Portfolio: %1 | NAV: %2 %3")
                 .arg(summary_.portfolio.name, summary_.portfolio.currency)
                 .arg(QString::number(summary_.total_market_value, 'f', 2));
    lines << QString("Total P&L: %1% | Day Change: %2%")
                 .arg(QString::number(summary_.total_unrealized_pnl_percent, 'f', 2))
                 .arg(QString::number(summary_.total_day_change_percent, 'f', 2));
    lines << QString("Holdings (%1):").arg(summary_.holdings.size());

    for (const auto& h : summary_.holdings) {
        lines << QString("  %1: %2 units @ %3 | P&L: %4% | Weight: %5%")
                     .arg(h.symbol)
                     .arg(QString::number(h.quantity, 'f', 2))
                     .arg(QString::number(h.current_price, 'f', 2))
                     .arg(QString::number(h.unrealized_pnl_percent, 'f', 2))
                     .arg(QString::number(h.weight, 'f', 1));
    }

    QString type_instruction;
    if (current_type_ == "risk")
        type_instruction =
            "Focus on risk analysis: identify concentration risks, correlation risks, and downside scenarios.";
    else if (current_type_ == "rebalance")
        type_instruction = "Suggest rebalancing actions with specific weight targets and rationale.";
    else if (current_type_ == "opportunities")
        type_instruction = "Identify opportunities: undervalued positions, missing sectors, and potential additions.";
    else
        type_instruction = "Provide a comprehensive portfolio analysis with actionable recommendations.";

    lines << "" << type_instruction;
    return lines.join("\n");
}

void PortfolioAiPanel::run_analysis(bool force) {
    if (analyzing_ || summary_.holdings.isEmpty())
        return;

    if (!force && result_cache_.contains(current_type_)) {
        content_->setHtml(result_cache_[current_type_]);
        return;
    }

    auto& llm = ai_chat::LlmService::instance();
    if (!llm.is_configured()) {
        content_->setPlainText("No LLM configured. Please add a model in Settings -> LLM Configuration.");
        return;
    }

    analyzing_ = true;
    run_btn_->setEnabled(false);
    status_lbl_->setText("\u23F3");
    content_->setPlainText(QString("Running %1 analysis...").arg(current_type_));

    QString prompt = build_portfolio_prompt();
    QString type = current_type_;
    QPointer<PortfolioAiPanel> self = this;

    QtConcurrent::run([self, prompt, type]() {
        auto response = ai_chat::LlmService::instance().chat(prompt, {});

        if (!self)
            return;
        QMetaObject::invokeMethod(
            self,
            [self, response, type]() {
                if (!self)
                    return;
                self->analyzing_ = false;
                self->run_btn_->setEnabled(true);
                self->status_lbl_->clear();

                if (response.success) {
                    // Simple markdown-to-html: preserve line breaks, bold
                    QString html = response.content;
                    html.replace("&", "&amp;");
                    html.replace("<", "&lt;");
                    html.replace(">", "&gt;");
                    html.replace("\n", "<br>");
                    static const QRegularExpression bold_re("\\*\\*(.+?)\\*\\*");
                    html.replace(bold_re, "<b>\\1</b>");
                    html = QString("<div style='font-size:11px; line-height:1.6; color:%1;'>%2</div>")
                               .arg(ui::colors::TEXT_PRIMARY)
                               .arg(html);

                    self->result_cache_[type] = html;
                    self->content_->setHtml(html);
                    self->run_btn_->setText(QString("\U0001F916 RE-RUN %1 ANALYSIS").arg(type.toUpper()));
                } else {
                    self->content_->setPlainText("Analysis failed: " + response.error);
                }
            },
            Qt::QueuedConnection);
    });
}

} // namespace fincept::screens
