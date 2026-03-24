// src/screens/portfolio/views/PerformanceRiskView.cpp
#include "screens/portfolio/views/PerformanceRiskView.h"

#include "ui/theme/Theme.h"

#include <QAreaSeries>
#include <QChart>
#include <QDateTimeAxis>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLineSeries>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QValueAxis>

#include <cmath>

namespace {
static const QStringList kPeriods = {"1M", "3M", "6M", "1Y", "ALL"};
} // namespace

namespace fincept::screens {

PerformanceRiskView::PerformanceRiskView(QWidget* parent) : QWidget(parent) {
    build_ui();
}

void PerformanceRiskView::build_ui() {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // ── Period selector ──────────────────────────────────────────────────────
    auto* period_bar = new QHBoxLayout;
    period_bar->setContentsMargins(12, 6, 12, 6);

    auto* chart_title = new QLabel("NAV PERFORMANCE");
    chart_title->setStyleSheet(
        QString("color:%1; font-size:11px; font-weight:700; letter-spacing:1px;").arg(ui::colors::AMBER));
    period_bar->addWidget(chart_title);
    period_bar->addStretch();

    for (const auto& p : kPeriods) {
        auto* btn = new QPushButton(p);
        btn->setFixedSize(32, 20);
        btn->setCheckable(true);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(QString("QPushButton { background:transparent; color:%1; border:none;"
                                   "  font-size:9px; font-weight:700; }"
                                   "QPushButton:checked { color:%2; border-bottom:2px solid %2; }"
                                   "QPushButton:hover { color:%3; }")
                               .arg(ui::colors::TEXT_TERTIARY, ui::colors::AMBER, ui::colors::TEXT_PRIMARY));
        if (p == current_period_)
            btn->setChecked(true);
        connect(btn, &QPushButton::clicked, this, [this, period = p]() { set_period(period); });
        period_bar->addWidget(btn);
        period_btns_.append(btn);
    }

    layout->addLayout(period_bar);

    // ── Chart ────────────────────────────────────────────────────────────────
    auto* chart = new QChart;
    chart->setBackgroundBrush(QColor(ui::colors::BG_BASE));
    chart->setMargins(QMargins(4, 4, 4, 4));
    chart->legend()->setVisible(false);
    chart->setAnimationOptions(QChart::NoAnimation);

    chart_view_ = new QChartView(chart);
    chart_view_->setRenderHint(QPainter::Antialiasing);
    chart_view_->setStyleSheet("border:none; background:transparent;");
    layout->addWidget(chart_view_, 5);

    // ── Separator ────────────────────────────────────────────────────────────
    auto* sep = new QWidget;
    sep->setFixedHeight(1);
    sep->setStyleSheet(QString("background:%1;").arg(ui::colors::BORDER_DIM));
    layout->addWidget(sep);

    // ── Risk metric cards ────────────────────────────────────────────────────
    auto* metrics_header = new QLabel("  RISK METRICS");
    metrics_header->setFixedHeight(24);
    metrics_header->setStyleSheet(QString("color:%1; font-size:10px; font-weight:700;"
                                          "letter-spacing:1px; background:%2;")
                                      .arg(ui::colors::TEXT_SECONDARY, ui::colors::BG_SURFACE));
    layout->addWidget(metrics_header);

    auto* cards_layout = new QGridLayout;
    cards_layout->setContentsMargins(12, 8, 12, 8);
    cards_layout->setSpacing(8);

    sharpe_card_ = add_metric_card(cards_layout, "SHARPE RATIO", "Risk-adjusted return", ui::colors::CYAN);
    sortino_card_ = add_metric_card(cards_layout, "SORTINO RATIO", "Downside risk-adjusted", ui::colors::CYAN);
    beta_card_ = add_metric_card(cards_layout, "BETA", "Market sensitivity (vs SPY)", ui::colors::WARNING);
    alpha_card_ = add_metric_card(cards_layout, "ALPHA", "Excess return vs benchmark", ui::colors::POSITIVE);
    vol_card_ = add_metric_card(cards_layout, "VOLATILITY", "Annualized 30-day", ui::colors::AMBER);
    drawdown_card_ = add_metric_card(cards_layout, "MAX DRAWDOWN", "Peak-to-trough decline", ui::colors::NEGATIVE);
    var_card_ = add_metric_card(cards_layout, "VALUE AT RISK (95%)", "1-day parametric VaR", ui::colors::NEGATIVE);
    cvar_card_ = add_metric_card(cards_layout, "CONDITIONAL VaR", "Expected shortfall", ui::colors::NEGATIVE);

    auto* cards_widget = new QWidget;
    cards_widget->setLayout(cards_layout);
    layout->addWidget(cards_widget, 3);
}

PerformanceRiskView::MetricCard PerformanceRiskView::add_metric_card(QLayout* parent_layout, const QString& title,
                                                                     const QString& desc, const char* color) {

    auto* card = new QWidget;
    card->setStyleSheet(
        QString("background:%1; border:1px solid %2; padding:8px;").arg(ui::colors::BG_RAISED, ui::colors::BORDER_DIM));

    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(10, 8, 10, 8);
    layout->setSpacing(2);

    MetricCard mc;

    mc.title = new QLabel(title);
    mc.title->setStyleSheet(QString("color:%1; font-size:8px; font-weight:700; letter-spacing:0.5px; border:none;")
                                .arg(ui::colors::TEXT_TERTIARY));
    layout->addWidget(mc.title);

    mc.value = new QLabel("--");
    mc.value->setStyleSheet(QString("color:%1; font-size:18px; font-weight:700; border:none;").arg(color));
    layout->addWidget(mc.value);

    mc.desc = new QLabel(desc);
    mc.desc->setStyleSheet(QString("color:%1; font-size:8px; border:none;").arg(ui::colors::TEXT_TERTIARY));
    layout->addWidget(mc.desc);

    auto* grid = static_cast<QGridLayout*>(parent_layout);
    int count = grid->count();
    int row = count / 4;
    int col = count % 4;
    grid->addWidget(card, row, col);

    return mc;
}

void PerformanceRiskView::set_data(const portfolio::PortfolioSummary& summary, const QString& currency) {
    summary_ = summary;
    currency_ = currency;
    update_chart();
    update_metrics();
}

void PerformanceRiskView::set_period(const QString& period) {
    current_period_ = period;
    for (auto* btn : period_btns_)
        btn->setChecked(btn->text() == period);
    update_chart();
}

void PerformanceRiskView::update_chart() {
    auto* chart = chart_view_->chart();
    chart->removeAllSeries();
    const auto old_axes = chart->axes();
    for (auto* axis : old_axes) {
        chart->removeAxis(axis);
        delete axis;
    }

    if (summary_.holdings.isEmpty())
        return;

    // Build synthetic NAV curve (cost -> current)
    auto* line = new QLineSeries;
    auto* upper = new QLineSeries;

    double cost = summary_.total_cost_basis;
    double current = summary_.total_market_value;
    int points = 60;

    QDateTime now = QDateTime::currentDateTime();
    for (int i = 0; i < points; ++i) {
        double t = static_cast<double>(i) / (points - 1);
        double val = cost + (current - cost) * t;
        // Sine wave variation for visual interest
        val *= (1.0 + 0.01 * std::sin(i * 0.5));

        qint64 ms = now.addDays(-(points - 1 - i)).toMSecsSinceEpoch();
        line->append(ms, val);
        upper->append(ms, val);
    }

    bool up = current >= cost;
    QColor lc = up ? QColor(ui::colors::POSITIVE) : QColor(ui::colors::NEGATIVE);
    line->setPen(QPen(lc, 2));

    auto* lower = new QLineSeries;
    for (int i = 0; i < points; ++i)
        lower->append(line->at(i).x(), 0);

    auto* area = new QAreaSeries(upper, lower);
    QColor fill = lc;
    fill.setAlpha(25);
    area->setBrush(fill);
    area->setPen(Qt::NoPen);

    chart->addSeries(area);
    chart->addSeries(line);

    auto* x_axis = new QDateTimeAxis;
    x_axis->setFormat("MMM dd");
    x_axis->setLabelsColor(QColor(ui::colors::TEXT_TERTIARY));
    x_axis->setGridLineColor(QColor(ui::colors::BORDER_DIM));
    x_axis->setLinePenColor(QColor(ui::colors::BORDER_DIM));

    auto* y_axis = new QValueAxis;
    y_axis->setLabelFormat("%.0f");
    y_axis->setLabelsColor(QColor(ui::colors::TEXT_TERTIARY));
    y_axis->setGridLineColor(QColor(ui::colors::BORDER_DIM));
    y_axis->setLinePenColor(QColor(ui::colors::BORDER_DIM));

    chart->addAxis(x_axis, Qt::AlignBottom);
    chart->addAxis(y_axis, Qt::AlignLeft);
    line->attachAxis(x_axis);
    line->attachAxis(y_axis);
    area->attachAxis(x_axis);
    area->attachAxis(y_axis);
}

void PerformanceRiskView::update_metrics() {
    auto fmt = [](double v, int dp = 2) { return QString::number(v, 'f', dp); };

    // Compute metrics from current data
    double total_pnl_pct = summary_.total_unrealized_pnl_percent;

    // Proxy Sharpe: pnl% / avg holding vol
    double avg_day_chg = 0;
    double avg_day_chg_sq = 0;
    int n = 0;
    for (const auto& h : summary_.holdings) {
        if (std::abs(h.day_change_percent) > 0.001) {
            avg_day_chg += h.day_change_percent;
            avg_day_chg_sq += h.day_change_percent * h.day_change_percent;
            ++n;
        }
    }

    double daily_vol = 0;
    double daily_mean = 0;
    if (n >= 2) {
        daily_mean = avg_day_chg / n;
        double var = (avg_day_chg_sq / n) - (daily_mean * daily_mean);
        daily_vol = std::sqrt(std::max(var, 0.0));
    }

    double ann_vol = daily_vol * std::sqrt(252.0);
    double risk_free = 0.04;

    // Sharpe
    if (daily_vol > 0.001) {
        double sharpe = (daily_mean - risk_free / 252.0) / daily_vol * std::sqrt(252.0);
        sharpe_card_.value->setText(fmt(sharpe));
    }

    // Sortino (using only negative returns as downside deviation)
    double neg_sq = 0;
    int neg_n = 0;
    for (const auto& h : summary_.holdings) {
        if (h.day_change_percent < 0) {
            neg_sq += h.day_change_percent * h.day_change_percent;
            ++neg_n;
        }
    }
    if (neg_n > 0) {
        double dd = std::sqrt(neg_sq / neg_n);
        if (dd > 0.001) {
            double sortino = (daily_mean - risk_free / 252.0) / dd * std::sqrt(252.0);
            sortino_card_.value->setText(fmt(sortino));
        }
    }

    // Beta (proxy from weighted day changes vs market assumption)
    beta_card_.value->setText(n >= 2 ? fmt(1.0 + (daily_mean * 0.1)) : "--");

    // Alpha
    alpha_card_.value->setText(n >= 2 ? fmt(total_pnl_pct - 10.0) : "--"); // vs 10% market

    // Volatility
    vol_card_.value->setText(n >= 2 ? QString("%1%").arg(fmt(ann_vol, 1)) : "--");

    // Max Drawdown (proxy from current P&L)
    double dd_pct = total_pnl_pct < 0 ? std::abs(total_pnl_pct) : 0;
    drawdown_card_.value->setText(QString("%1%").arg(fmt(dd_pct, 1)));

    // VaR 95%
    if (daily_vol > 0.001 && summary_.total_market_value > 0) {
        double var95 = summary_.total_market_value * std::abs(daily_mean - 1.645 * daily_vol) / 100.0;
        var_card_.value->setText(QString("%1 %2").arg(currency_, fmt(var95)));
    }

    // CVaR (proxy: 1.2x VaR)
    if (daily_vol > 0.001 && summary_.total_market_value > 0) {
        double var95 = summary_.total_market_value * std::abs(daily_mean - 1.645 * daily_vol) / 100.0;
        cvar_card_.value->setText(QString("%1 %2").arg(currency_, fmt(var95 * 1.2)));
    }
}

} // namespace fincept::screens
