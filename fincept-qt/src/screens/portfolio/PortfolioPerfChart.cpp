// src/screens/portfolio/PortfolioPerfChart.cpp
#include "screens/portfolio/PortfolioPerfChart.h"

#include "ui/theme/Theme.h"

#include <QAreaSeries>
#include <QChart>
#include <QDateTimeAxis>
#include <QHBoxLayout>
#include <QLineSeries>
#include <QVBoxLayout>
#include <QValueAxis>

#include <cmath>

namespace {
static const QStringList kPeriods = {"1D", "1W", "1M", "3M", "YTD", "1Y", "ALL"};
} // namespace

namespace fincept::screens {

PortfolioPerfChart::PortfolioPerfChart(QWidget* parent) : QWidget(parent) {
    build_ui();
}

void PortfolioPerfChart::build_ui() {
    setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE));

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Header: PERFORMANCE + period buttons
    auto* header = new QHBoxLayout;
    header->setContentsMargins(8, 4, 8, 4);

    auto* title = new QLabel("PERFORMANCE");
    title->setStyleSheet(
        QString("color:%1; font-size:10px; font-weight:700; letter-spacing:1px;").arg(ui::colors::TEXT_SECONDARY));
    header->addWidget(title);
    header->addStretch();

    for (const auto& p : kPeriods) {
        auto* btn = new QPushButton(p);
        btn->setFixedSize(28, 18);
        btn->setCheckable(true);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(QString("QPushButton { background:transparent; color:%1; border:none;"
                                   "  font-size:8px; font-weight:700; }"
                                   "QPushButton:checked { color:%2; border-bottom:1px solid %2; }"
                                   "QPushButton:hover { color:%3; }")
                               .arg(ui::colors::TEXT_TERTIARY, ui::colors::AMBER, ui::colors::TEXT_PRIMARY));

        if (p == current_period_)
            btn->setChecked(true);

        connect(btn, &QPushButton::clicked, this, [this, period = p]() { set_period(period); });

        header->addWidget(btn);
        period_btns_.append(btn);
    }

    layout->addLayout(header);

    // Info bar: period change, total return, NAV
    auto* info_bar = new QHBoxLayout;
    info_bar->setContentsMargins(8, 0, 8, 4);
    info_bar->setSpacing(12);

    period_change_label_ = new QLabel;
    period_change_label_->setStyleSheet(
        QString("color:%1; font-size:11px; font-weight:600;").arg(ui::colors::TEXT_PRIMARY));
    info_bar->addWidget(period_change_label_);

    auto* sep1 = new QLabel("|");
    sep1->setStyleSheet(QString("color:%1;").arg(ui::colors::BORDER_MED));
    info_bar->addWidget(sep1);

    total_return_label_ = new QLabel;
    total_return_label_->setStyleSheet(
        QString("color:%1; font-size:14px; font-weight:700;").arg(ui::colors::TEXT_PRIMARY));
    info_bar->addWidget(total_return_label_);

    auto* sep2 = new QLabel("|");
    sep2->setStyleSheet(QString("color:%1;").arg(ui::colors::BORDER_MED));
    info_bar->addWidget(sep2);

    nav_label_ = new QLabel;
    nav_label_->setStyleSheet(QString("color:%1; font-size:11px; font-weight:600;").arg(ui::colors::WARNING));
    info_bar->addWidget(nav_label_);

    info_bar->addStretch();
    layout->addLayout(info_bar);

    // Chart view
    auto* chart = new QChart;
    chart->setBackgroundBrush(QColor(ui::colors::BG_BASE));
    chart->setMargins(QMargins(0, 0, 0, 0));
    chart->legend()->setVisible(false);
    chart->setAnimationOptions(QChart::NoAnimation);

    chart_view_ = new QChartView(chart);
    chart_view_->setRenderHint(QPainter::Antialiasing);
    chart_view_->setStyleSheet("border:none; background:transparent;");
    layout->addWidget(chart_view_, 1);
}

void PortfolioPerfChart::set_summary(const portfolio::PortfolioSummary& summary) {
    summary_ = summary;
    update_chart();
}

void PortfolioPerfChart::set_currency(const QString& currency) {
    currency_ = currency;
}

void PortfolioPerfChart::set_period(const QString& period) {
    current_period_ = period;
    for (auto* btn : period_btns_)
        btn->setChecked(btn->text() == period);
    update_chart();
}

QColor PortfolioPerfChart::chart_color() const {
    return summary_.total_unrealized_pnl >= 0 ? QColor(ui::colors::POSITIVE) : QColor(ui::colors::NEGATIVE);
}

void PortfolioPerfChart::update_chart() {
    auto* chart = chart_view_->chart();
    chart->removeAllSeries();

    // Remove and delete old axes (removeAxis doesn't delete)
    const auto old_axes = chart->axes();
    for (auto* axis : old_axes) {
        chart->removeAxis(axis);
        delete axis;
    }

    if (summary_.holdings.isEmpty()) {
        period_change_label_->setText("No data");
        total_return_label_->clear();
        nav_label_->clear();
        return;
    }

    // Build a simple value series: cost basis -> current value
    // Real historical data would come from snapshots; for now use a synthetic curve
    auto* line = new QLineSeries;
    auto* area_upper = new QLineSeries;

    double cost = summary_.total_cost_basis;
    double current = summary_.total_market_value;
    int points = 30; // synthetic data points

    QDateTime now = QDateTime::currentDateTime();
    for (int i = 0; i < points; ++i) {
        double t = static_cast<double>(i) / (points - 1);
        double val = cost + (current - cost) * t;
        // Add a bit of variation
        double noise = (i % 3 == 0) ? -0.005 : (i % 3 == 1) ? 0.003 : 0;
        val *= (1.0 + noise);

        qint64 ms = now.addDays(-(points - 1 - i)).toMSecsSinceEpoch();
        line->append(ms, val);
        area_upper->append(ms, val);
    }

    QColor lc = chart_color();
    QPen pen(lc, 2);
    line->setPen(pen);

    // Area fill
    auto* area_lower = new QLineSeries;
    for (int i = 0; i < points; ++i) {
        area_lower->append(line->at(i).x(), 0);
    }

    auto* area = new QAreaSeries(area_upper, area_lower);
    QColor fill = lc;
    fill.setAlpha(30);
    area->setBrush(fill);
    area->setPen(Qt::NoPen);

    chart->addSeries(area);
    chart->addSeries(line);

    // Axes
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

    // Update info labels
    double pnl_pct = summary_.total_unrealized_pnl_percent;
    const char* pnl_color = pnl_pct >= 0 ? ui::colors::POSITIVE : ui::colors::NEGATIVE;

    period_change_label_->setText(
        QString("PERIOD %1%2%").arg(pnl_pct >= 0 ? "+" : "").arg(QString::number(pnl_pct, 'f', 2)));
    period_change_label_->setStyleSheet(QString("color:%1; font-size:11px; font-weight:600;").arg(pnl_color));

    total_return_label_->setText(
        QString("TOTAL RETURN %1%2%").arg(pnl_pct >= 0 ? "+" : "").arg(QString::number(pnl_pct, 'f', 2)));
    total_return_label_->setStyleSheet(QString("color:%1; font-size:14px; font-weight:700;").arg(pnl_color));

    nav_label_->setText(QString("NAV %1 %2").arg(currency_).arg(QString::number(summary_.total_market_value, 'f', 2)));
}

} // namespace fincept::screens
