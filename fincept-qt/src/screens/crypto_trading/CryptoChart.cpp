// CryptoChart.cpp — Candlestick chart with TF toggles, vibrant colors
#include "screens/crypto_trading/CryptoChart.h"

#include <QCandlestickSeries>
#include <QCandlestickSet>
#include <QChart>
#include <QChartView>
#include <QDateTime>
#include <QDateTimeAxis>
#include <QHBoxLayout>
#include <QLabel>
#include <QStyle>
#include <QVBoxLayout>
#include <QValueAxis>

#include <cmath>

namespace fincept::screens::crypto {

constexpr const char* CryptoChart::TF_LABELS[];

CryptoChart::CryptoChart(QWidget* parent) : QWidget(parent) {
    setObjectName("cryptoChart");

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Header with TF toggle buttons
    auto* header = new QWidget;
    header->setObjectName("cryptoChartHeader");
    header->setFixedHeight(28);
    auto* h_layout = new QHBoxLayout(header);
    h_layout->setContentsMargins(8, 0, 8, 0);
    h_layout->setSpacing(2);

    auto* title = new QLabel("CHART");
    title->setObjectName("cryptoChartTitle");
    h_layout->addWidget(title);
    h_layout->addSpacing(8);

    for (int i = 0; i < 6; ++i) {
        tf_buttons_[i] = new QPushButton(TF_LABELS[i]);
        tf_buttons_[i]->setObjectName("cryptoTfBtn");
        tf_buttons_[i]->setFixedHeight(22);
        tf_buttons_[i]->setCursor(Qt::PointingHandCursor);
        if (i == active_tf_)
            tf_buttons_[i]->setProperty("active", true);
        connect(tf_buttons_[i], &QPushButton::clicked, this, [this, i]() { set_active_tf(i); });
        h_layout->addWidget(tf_buttons_[i]);
    }
    h_layout->addStretch();
    layout->addWidget(header);

    // Chart setup
    chart_ = new QChart;
    chart_->setBackgroundBrush(QBrush(QColor("#0a0a0a")));
    chart_->setPlotAreaBackgroundBrush(QBrush(QColor("#060606")));
    chart_->setPlotAreaBackgroundVisible(true);
    chart_->legend()->hide();
    chart_->setMargins(QMargins(4, 4, 4, 4));

    series_ = new QCandlestickSeries;
    // Vibrant candle colors
    series_->setIncreasingColor(QColor("#22c55e")); // bright green
    series_->setDecreasingColor(QColor("#ef4444")); // bright red
    series_->setBodyWidth(0.75);
    series_->setPen(QPen(Qt::NoPen)); // no outline on body
    series_->setCapsVisible(false);   // clean look
    chart_->addSeries(series_);

    time_axis_ = new QDateTimeAxis;
    time_axis_->setFormat("HH:mm");
    time_axis_->setLabelsColor(QColor("#404040"));
    time_axis_->setGridLineColor(QColor("#151515"));
    time_axis_->setMinorGridLineColor(QColor("#0e0e0e"));
    time_axis_->setLinePenColor(QColor("#1a1a1a"));
    chart_->addAxis(time_axis_, Qt::AlignBottom);
    series_->attachAxis(time_axis_);

    price_axis_ = new QValueAxis;
    price_axis_->setLabelsColor(QColor("#404040"));
    price_axis_->setGridLineColor(QColor("#151515"));
    price_axis_->setMinorGridLineColor(QColor("#0e0e0e"));
    price_axis_->setLinePenColor(QColor("#1a1a1a"));
    price_axis_->setLabelFormat("%.1f");
    chart_->addAxis(price_axis_, Qt::AlignRight);
    series_->attachAxis(price_axis_);

    chart_view_ = new QChartView(chart_);
    chart_view_->setRenderHint(QPainter::Antialiasing, false);
    layout->addWidget(chart_view_, 1);

    // Minimum height so chart is never too small
    setMinimumHeight(300);
}

void CryptoChart::set_active_tf(int idx) {
    active_tf_ = idx;
    for (int i = 0; i < 6; ++i) {
        tf_buttons_[i]->setProperty("active", i == idx);
        // setProperty + style()->polish is expensive (12 CSS recalcs for 6 buttons).
        // Since the global stylesheet already selects on [active=true], we only need
        // to trigger a repaint after the property change. Qt's dynamic property change
        // already triggers a style recalc when the next paint occurs.
        tf_buttons_[i]->update();
    }
    emit timeframe_changed(TF_LABELS[idx]);
}

QString CryptoChart::current_timeframe() const {
    return TF_LABELS[active_tf_];
}

void CryptoChart::set_candles(const QVector<trading::Candle>& candles) {
    candles_ = candles;
    rebuild_chart();
}

void CryptoChart::append_candle(const trading::Candle& candle) {
    const auto sets = series_->sets();
    const bool is_update = !candles_.isEmpty() && candles_.last().timestamp == candle.timestamp;

    if (is_update) {
        candles_.last() = candle;
        if (!sets.isEmpty()) {
            auto* last_set = sets.last();
            last_set->setOpen(candle.open);
            last_set->setHigh(candle.high);
            last_set->setLow(candle.low);
            last_set->setClose(candle.close);
        }
    } else {
        candles_.append(candle);
        if (candles_.size() > MAX_VISIBLE * 2) {
            candles_.remove(0, candles_.size() - MAX_VISIBLE);
            rebuild_chart();
            return;
        }
        if (sets.size() >= MAX_VISIBLE)
            series_->remove(sets.first());
        series_->append(new QCandlestickSet(candle.open, candle.high, candle.low, candle.close, candle.timestamp));
    }

    const auto& visible_sets = series_->sets();
    if (visible_sets.isEmpty())
        return;

    double min_price = 1e18, max_price = 0;
    qint64 min_time = INT64_MAX, max_time = 0;
    for (const auto* s : visible_sets) {
        min_price = std::min(min_price, s->low());
        max_price = std::max(max_price, s->high());
        const qint64 ts = static_cast<qint64>(s->timestamp());
        if (ts < min_time)
            min_time = ts;
        if (ts > max_time)
            max_time = ts;
    }
    update_axes(min_price, max_price, min_time, max_time);
}

void CryptoChart::clear() {
    candles_.clear();
    series_->clear();
}

void CryptoChart::update_axes(double min_price, double max_price, qint64 min_time, qint64 max_time) {
    if (min_price >= max_price)
        return; // invalid range

    const double padding = (max_price - min_price) * 0.05;
    const double p_min = min_price - padding;
    const double p_max = max_price + padding;

    if (p_min != last_min_price_ || p_max != last_max_price_) {
        price_axis_->setRange(p_min, p_max);
        last_min_price_ = p_min;
        last_max_price_ = p_max;
    }
    if (min_time != last_min_time_ || max_time != last_max_time_) {
        time_axis_->setRange(QDateTime::fromMSecsSinceEpoch(min_time), QDateTime::fromMSecsSinceEpoch(max_time));
        last_min_time_ = min_time;
        last_max_time_ = max_time;
    }
}

void CryptoChart::rebuild_chart() {
    series_->clear();
    last_min_price_ = last_max_price_ = -1;
    last_min_time_ = last_max_time_ = -1;

    if (candles_.isEmpty())
        return;

    const int start = std::max(0, static_cast<int>(candles_.size()) - MAX_VISIBLE);
    double min_price = 1e18, max_price = 0;
    qint64 min_time = INT64_MAX, max_time = 0;

    for (int i = start; i < candles_.size(); ++i) {
        const auto& c = candles_[i];
        series_->append(new QCandlestickSet(c.open, c.high, c.low, c.close, c.timestamp));
        min_price = std::min(min_price, c.low);
        max_price = std::max(max_price, c.high);
        if (c.timestamp < min_time)
            min_time = c.timestamp;
        if (c.timestamp > max_time)
            max_time = c.timestamp;
    }

    update_axes(min_price, max_price, min_time, max_time);
}

} // namespace fincept::screens::crypto
