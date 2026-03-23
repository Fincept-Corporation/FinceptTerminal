// src/screens/portfolio/PortfolioDetailWrapper.cpp
#include "screens/portfolio/PortfolioDetailWrapper.h"
#include "screens/portfolio/views/AnalyticsSectorsView.h"
#include "screens/portfolio/views/PerformanceRiskView.h"
#include "screens/portfolio/views/PortfolioOptimizationView.h"
#include "screens/portfolio/views/QuantStatsView.h"
#include "screens/portfolio/views/ReportsView.h"
#include "screens/portfolio/views/RiskManagementView.h"
#include "screens/portfolio/views/CustomIndexView.h"
#include "screens/portfolio/views/PlanningView.h"
#include "screens/portfolio/views/EconomicsView.h"
#include "ui/theme/Theme.h"

#include <QHBoxLayout>
#include <QVBoxLayout>

namespace fincept::screens {

PortfolioDetailWrapper::PortfolioDetailWrapper(QWidget* parent) : QWidget(parent) {
    build_ui();
}

void PortfolioDetailWrapper::build_ui() {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // ── Header bar (36px) ────────────────────────────────────────────────────
    auto* header = new QWidget;
    header->setFixedHeight(36);
    header->setStyleSheet(QString(
        "background: qlineargradient(x1:0,x2:1, stop:0 %1, stop:1 %2);"
        "border-bottom:1px solid %3;")
        .arg(ui::colors::BG_RAISED, ui::colors::BG_SURFACE, ui::colors::AMBER));

    auto* h_layout = new QHBoxLayout(header);
    h_layout->setContentsMargins(8, 0, 8, 0);
    h_layout->setSpacing(8);

    // Back button
    back_btn_ = new QPushButton("\u2190 BACK");
    back_btn_->setFixedHeight(24);
    back_btn_->setCursor(Qt::PointingHandCursor);
    back_btn_->setStyleSheet(QString(
        "QPushButton { background:transparent; color:%1; border:1px solid %1;"
        "  padding:0 10px; font-size:9px; font-weight:700; letter-spacing:0.5px; }"
        "QPushButton:hover { background:%1; color:#000; }")
        .arg(ui::colors::AMBER));
    connect(back_btn_, &QPushButton::clicked, this, &PortfolioDetailWrapper::back_requested);
    h_layout->addWidget(back_btn_);

    // Vertical divider
    auto* div = new QWidget;
    div->setFixedSize(1, 20);
    div->setStyleSheet(QString("background:%1;").arg(ui::colors::BORDER_MED));
    h_layout->addWidget(div);

    // View title
    title_label_ = new QLabel;
    title_label_->setStyleSheet(QString("color:%1; font-size:11px; font-weight:700; letter-spacing:1px;")
                                .arg(ui::colors::AMBER));
    h_layout->addWidget(title_label_);

    h_layout->addStretch();

    // Portfolio info (right side)
    portfolio_label_ = new QLabel;
    portfolio_label_->setStyleSheet(QString("color:%1; font-size:10px; font-weight:600;")
                                    .arg(ui::colors::TEXT_SECONDARY));
    h_layout->addWidget(portfolio_label_);

    layout->addWidget(header);

    // ── View stack ───────────────────────────────────────────────────────────
    view_stack_ = new QStackedWidget;
    view_stack_->setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE));
    layout->addWidget(view_stack_, 1);
}

void PortfolioDetailWrapper::show_view(portfolio::DetailView view,
                                         const portfolio::PortfolioSummary& summary,
                                         const QString& currency) {
    current_view_ = view;
    current_summary_ = summary;
    current_currency_ = currency;

    title_label_->setText(view_title(view));
    portfolio_label_->setText(QString("\U0001F4BC %1 | %2")
        .arg(summary.portfolio.name, summary.portfolio.currency));

    auto* widget = get_or_create_view(view);
    view_stack_->setCurrentWidget(widget);

    update_data(summary, currency);
}

void PortfolioDetailWrapper::update_data(const portfolio::PortfolioSummary& summary,
                                           const QString& currency) {
    current_summary_ = summary;
    current_currency_ = currency;

    auto* current = view_stack_->currentWidget();
    if (!current) return;

    // Update the active view with new data
    if (auto* v = qobject_cast<AnalyticsSectorsView*>(current)) {
        v->set_data(summary, currency);
    } else if (auto* v = qobject_cast<PerformanceRiskView*>(current)) {
        v->set_data(summary, currency);
    } else if (auto* v = qobject_cast<RiskManagementView*>(current)) {
        v->set_data(summary, currency);
    } else if (auto* v = qobject_cast<PortfolioOptimizationView*>(current)) {
        v->set_data(summary, currency);
    } else if (auto* v = qobject_cast<QuantStatsView*>(current)) {
        v->set_data(summary, currency);
    } else if (auto* v = qobject_cast<ReportsView*>(current)) {
        v->set_data(summary, currency);
    } else if (auto* v = qobject_cast<CustomIndexView*>(current)) {
        v->set_data(summary, currency);
    } else if (auto* v = qobject_cast<PlanningView*>(current)) {
        v->set_data(summary, currency);
    } else if (auto* v = qobject_cast<EconomicsView*>(current)) {
        v->set_data(summary, currency);
    }
}

QWidget* PortfolioDetailWrapper::get_or_create_view(portfolio::DetailView view) {
    int key = static_cast<int>(view);
    auto it = views_.find(key);
    if (it != views_.end())
        return *it;

    QWidget* widget = nullptr;
    switch (view) {
        case portfolio::DetailView::AnalyticsSectors:
            widget = new AnalyticsSectorsView;
            break;
        case portfolio::DetailView::PerfRisk:
            widget = new PerformanceRiskView;
            break;
        case portfolio::DetailView::RiskMgmt:
            widget = new RiskManagementView;
            break;
        case portfolio::DetailView::Optimization:
            widget = new PortfolioOptimizationView;
            break;
        case portfolio::DetailView::QuantStats:
            widget = new QuantStatsView;
            break;
        case portfolio::DetailView::ReportsPme:
            widget = new ReportsView;
            break;
        case portfolio::DetailView::Indices:
            widget = new CustomIndexView;
            break;
        case portfolio::DetailView::Planning:
            widget = new PlanningView;
            break;
        case portfolio::DetailView::Economics:
            widget = new EconomicsView;
            break;
    }

    view_stack_->addWidget(widget);
    views_[key] = widget;
    return widget;
}

QString PortfolioDetailWrapper::view_title(portfolio::DetailView view) const {
    switch (view) {
        case portfolio::DetailView::AnalyticsSectors: return "ANALYTICS & SECTORS";
        case portfolio::DetailView::PerfRisk:         return "PERFORMANCE & RISK";
        case portfolio::DetailView::Optimization:     return "PORTFOLIO OPTIMIZATION";
        case portfolio::DetailView::QuantStats:       return "QUANTSTATS REPORT";
        case portfolio::DetailView::ReportsPme:       return "REPORTS & PME";
        case portfolio::DetailView::Indices:          return "CUSTOM INDICES";
        case portfolio::DetailView::RiskMgmt:         return "RISK MANAGEMENT";
        case portfolio::DetailView::Planning:         return "FINANCIAL PLANNING";
        case portfolio::DetailView::Economics:        return "ECONOMICS";
    }
    return "DETAIL VIEW";
}

} // namespace fincept::screens
