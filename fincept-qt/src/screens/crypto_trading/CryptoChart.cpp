#include "screens/crypto_trading/CryptoChart.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QChartView>
#include <QChart>
#include <QCandlestickSeries>
#include <QCandlestickSet>
#include <QDateTimeAxis>
#include <QValueAxis>
#include <QDateTime>
#include <cmath>

namespace fincept::screens::crypto {

CryptoChart::CryptoChart(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);

    // Timeframe selector
    auto* tf_bar = new QHBoxLayout;
    tf_bar->setSpacing(4);

    auto* chart_label = new QLabel("CHART");
    chart_label->setStyleSheet("font-weight: bold; color: #00aaff; font-size: 12px;");
    tf_bar->addWidget(chart_label);

    timeframe_combo_ = new QComboBox;
    timeframe_combo_->addItems({"1m", "5m", "15m", "1h", "4h", "1d"});
    timeframe_combo_->setCurrentText("1h");
    timeframe_combo_->setFixedWidth(70);
    connect(timeframe_combo_, &QComboBox::currentTextChanged, this, &CryptoChart::timeframe_changed);
    tf_bar->addWidget(timeframe_combo_);
    tf_bar->addStretch();

    layout->addLayout(tf_bar);

    // Chart
    chart_ = new QChart;
    chart_->setBackgroundBrush(QBrush(QColor("#0d1117")));
    chart_->setPlotAreaBackgroundBrush(QBrush(QColor("#0d1117")));
    chart_->setPlotAreaBackgroundVisible(true);
    chart_->legend()->hide();
    chart_->setMargins(QMargins(0, 0, 0, 0));

    series_ = new QCandlestickSeries;
    series_->setIncreasingColor(QColor("#00ff88"));
    series_->setDecreasingColor(QColor("#ff4444"));
    series_->setBodyWidth(0.7);
    chart_->addSeries(series_);

    time_axis_ = new QDateTimeAxis;
    time_axis_->setFormat("HH:mm");
    time_axis_->setLabelsColor(QColor("#888"));
    time_axis_->setGridLineColor(QColor("#1a1a2e"));
    chart_->addAxis(time_axis_, Qt::AlignBottom);
    series_->attachAxis(time_axis_);

    price_axis_ = new QValueAxis;
    price_axis_->setLabelsColor(QColor("#888"));
    price_axis_->setGridLineColor(QColor("#1a1a2e"));
    chart_->addAxis(price_axis_, Qt::AlignRight);
    series_->attachAxis(price_axis_);

    chart_view_ = new QChartView(chart_);
    chart_view_->setRenderHint(QPainter::Antialiasing);
    chart_view_->setStyleSheet("background: #0d1117; border: 1px solid #222; border-radius: 4px;");
    layout->addWidget(chart_view_, 1);
}

QString CryptoChart::current_timeframe() const {
    return timeframe_combo_->currentText();
}

void CryptoChart::set_candles(const QVector<trading::Candle>& candles) {
    candles_ = candles;
    rebuild_chart();
}

void CryptoChart::append_candle(const trading::Candle& candle) {
    if (!candles_.isEmpty() && candles_.last().timestamp == candle.timestamp) {
        candles_.last() = candle; // update in-place
    } else {
        candles_.append(candle);
        if (candles_.size() > MAX_VISIBLE * 2) {
            candles_.remove(0, candles_.size() - MAX_VISIBLE);
        }
    }
    rebuild_chart();
}

void CryptoChart::clear() {
    candles_.clear();
    series_->clear();
}

void CryptoChart::rebuild_chart() {
    series_->clear();
    if (candles_.isEmpty()) return;

    int start = std::max(0, static_cast<int>(candles_.size()) - MAX_VISIBLE);
    double min_price = 1e18, max_price = 0;
    qint64 min_time = INT64_MAX, max_time = 0;

    for (int i = start; i < candles_.size(); ++i) {
        const auto& c = candles_[i];
        auto* set = new QCandlestickSet(c.open, c.high, c.low, c.close, c.timestamp);
        series_->append(set);

        min_price = std::min(min_price, c.low);
        max_price = std::max(max_price, c.high);
        if (c.timestamp < min_time) min_time = c.timestamp;
        if (c.timestamp > max_time) max_time = c.timestamp;
    }

    double padding = (max_price - min_price) * 0.05;
    price_axis_->setRange(min_price - padding, max_price + padding);
    time_axis_->setRange(
        QDateTime::fromMSecsSinceEpoch(min_time),
        QDateTime::fromMSecsSinceEpoch(max_time));
}

} // namespace fincept::screens::crypto
