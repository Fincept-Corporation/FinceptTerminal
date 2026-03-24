// EquityChart.cpp — Candlestick chart with timeframe buttons
#include "screens/equity_trading/EquityChart.h"

#include "screens/equity_trading/EquityTypes.h"

#include <QCandlestickSeries>
#include <QCandlestickSet>
#include <QChart>
#include <QChartView>
#include <QDateTimeAxis>
#include <QHBoxLayout>
#include <QStyle>
#include <QVBoxLayout>
#include <QValueAxis>

#include <algorithm>
#include <cmath>

namespace fincept::screens::equity {

EquityChart::EquityChart(QWidget* parent) : QWidget(parent) {
    setObjectName("eqChart");

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Header with timeframe buttons
    auto* header = new QWidget;
    header->setObjectName("eqChartHeader");
    header->setFixedHeight(28);
    auto* h_layout = new QHBoxLayout(header);
    h_layout->setContentsMargins(8, 0, 8, 0);
    h_layout->setSpacing(2);

    auto* title = new QLabel("CHART");
    title->setObjectName("eqChartTitle");
    h_layout->addWidget(title);
    h_layout->addStretch();

    for (int i = 0; i < 6; ++i) {
        tf_buttons_[i] = new QPushButton(TF_LABELS[i]);
        tf_buttons_[i]->setObjectName("eqTfBtn");
        tf_buttons_[i]->setFixedHeight(20);
        tf_buttons_[i]->setCursor(Qt::PointingHandCursor);
        if (i == active_tf_)
            tf_buttons_[i]->setProperty("active", true);
        connect(tf_buttons_[i], &QPushButton::clicked, this, [this, i]() { set_active_tf(i); });
        h_layout->addWidget(tf_buttons_[i]);
    }
    layout->addWidget(header);

    // Chart
    chart_ = new QChart;
    chart_->setBackgroundBrush(BG_SURFACE);
    chart_->legend()->hide();
    chart_->setMargins(QMargins(0, 0, 0, 0));

    series_ = new QCandlestickSeries;
    series_->setIncreasingColor(COLOR_BUY);
    series_->setDecreasingColor(COLOR_SELL);
    series_->setBodyWidth(0.7);
    series_->setPen(Qt::NoPen);
    chart_->addSeries(series_);

    time_axis_ = new QDateTimeAxis;
    time_axis_->setFormat("HH:mm");
    time_axis_->setLabelsColor(TEXT_SECONDARY);
    time_axis_->setGridLineColor(BORDER_DIM);
    chart_->addAxis(time_axis_, Qt::AlignBottom);
    series_->attachAxis(time_axis_);

    price_axis_ = new QValueAxis;
    price_axis_->setLabelsColor(TEXT_SECONDARY);
    price_axis_->setGridLineColor(BORDER_DIM);
    price_axis_->setLabelFormat("%.2f");
    chart_->addAxis(price_axis_, Qt::AlignRight);
    series_->attachAxis(price_axis_);

    chart_view_ = new QChartView(chart_);
    chart_view_->setRenderHint(QPainter::Antialiasing, false);
    chart_view_->setObjectName("eqChartView");
    layout->addWidget(chart_view_, 1);
}

void EquityChart::set_candles(const QVector<trading::BrokerCandle>& candles) {
    candles_ = candles;
    rebuild_chart();
}

void EquityChart::clear() {
    candles_.clear();
    series_->clear();
}

QString EquityChart::current_timeframe() const {
    return TF_LABELS[active_tf_];
}

void EquityChart::set_active_tf(int idx) {
    if (idx == active_tf_)
        return;
    for (int i = 0; i < 6; ++i) {
        tf_buttons_[i]->setProperty("active", i == idx);
        tf_buttons_[i]->style()->unpolish(tf_buttons_[i]);
        tf_buttons_[i]->style()->polish(tf_buttons_[i]);
    }
    active_tf_ = idx;
    emit timeframe_changed(TF_LABELS[idx]);
}

void EquityChart::rebuild_chart() {
    series_->clear();
    last_min_price_ = last_max_price_ = -1;
    last_min_time_ = last_max_time_ = -1;

    if (candles_.isEmpty())
        return;

    const int start = qMax(0, static_cast<int>(candles_.size()) - MAX_VISIBLE);
    double min_p = 1e18, max_p = 0;
    qint64 min_t = INT64_MAX, max_t = 0;

    for (int i = start; i < candles_.size(); ++i) {
        const auto& c = candles_[i];
        auto* set = new QCandlestickSet(c.open, c.high, c.low, c.close, c.timestamp * 1000);
        series_->append(set);
        min_p = std::min(min_p, c.low);
        max_p = std::max(max_p, c.high);
        min_t = std::min(min_t, c.timestamp * 1000);
        max_t = std::max(max_t, c.timestamp * 1000);
    }

    if (min_p < max_p)
        update_axes(min_p, max_p, min_t, max_t);
}

void EquityChart::update_axes(double min_price, double max_price, qint64 min_time, qint64 max_time) {
    if (min_price == last_min_price_ && max_price == last_max_price_ && min_time == last_min_time_ &&
        max_time == last_max_time_)
        return;

    const double margin = (max_price - min_price) * 0.05;
    price_axis_->setRange(min_price - margin, max_price + margin);
    time_axis_->setRange(QDateTime::fromMSecsSinceEpoch(min_time), QDateTime::fromMSecsSinceEpoch(max_time));

    last_min_price_ = min_price;
    last_max_price_ = max_price;
    last_min_time_ = min_time;
    last_max_time_ = max_time;
}

} // namespace fincept::screens::equity
