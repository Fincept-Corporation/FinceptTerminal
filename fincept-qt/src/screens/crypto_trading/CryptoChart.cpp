// CryptoChart.cpp — Candlestick chart with TF toggles, vibrant colors
#include "screens/crypto_trading/CryptoChart.h"

#include "ui/theme/Theme.h"

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

using namespace fincept::ui;

namespace fincept::screens::crypto {

constexpr const char* CryptoChart::TF_LABELS[];

CryptoChart::CryptoChart(QWidget* parent) : QWidget(parent) {
    setObjectName("cryptoChart");

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Header with TF toggle buttons
    auto* header = new QWidget(this);
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
    chart_->setBackgroundBrush(QBrush(QColor(colors::BG_SURFACE())));
    chart_->setPlotAreaBackgroundBrush(QBrush(QColor(colors::BG_BASE())));
    chart_->setPlotAreaBackgroundVisible(true);
    chart_->legend()->hide();
    chart_->setMargins(QMargins(4, 4, 4, 4));

    series_ = new QCandlestickSeries;
    // Vibrant candle colors
    series_->setIncreasingColor(QColor(colors::POSITIVE()));
    series_->setDecreasingColor(QColor(colors::NEGATIVE()));
    series_->setBodyWidth(0.75);
    series_->setPen(QPen(Qt::NoPen)); // no outline on body
    series_->setCapsVisible(false);   // clean look
    chart_->addSeries(series_);

    time_axis_ = new QDateTimeAxis;
    time_axis_->setFormat("HH:mm");
    time_axis_->setLabelsColor(QColor(colors::TEXT_DIM()));
    time_axis_->setGridLineColor(QColor(colors::BG_RAISED()));
    time_axis_->setMinorGridLineColor(QColor(colors::BG_SURFACE()));
    time_axis_->setLinePenColor(QColor(colors::BORDER_DIM()));
    chart_->addAxis(time_axis_, Qt::AlignBottom);
    series_->attachAxis(time_axis_);

    price_axis_ = new QValueAxis;
    price_axis_->setLabelsColor(QColor(colors::TEXT_DIM()));
    price_axis_->setGridLineColor(QColor(colors::BG_RAISED()));
    price_axis_->setMinorGridLineColor(QColor(colors::BG_SURFACE()));
    price_axis_->setLinePenColor(QColor(colors::BORDER_DIM()));
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
    // Optimistically update the button highlight so the user gets immediate feedback.
    for (int i = 0; i < 6; ++i) {
        tf_buttons_[i]->setProperty("active", i == idx);
        tf_buttons_[i]->update();
    }
    // Store as pending — if candles_fetching_ is true in the screen, the fetch will
    // be dropped. set_candles() re-emits timeframe_changed with pending_tf_ so the
    // screen retries after the in-flight fetch completes.
    pending_tf_ = TF_LABELS[idx];
    active_tf_ = idx;
    emit timeframe_changed(TF_LABELS[idx]);
}

QString CryptoChart::current_timeframe() const {
    return TF_LABELS[active_tf_];
}

void CryptoChart::set_candles(const QVector<trading::Candle>& candles) {
    candles_ = candles;
    rebuild_chart();

    // If the user changed the timeframe while the previous fetch was in-flight,
    // the screen's candles_fetching_ guard would have dropped that request.
    // Re-emit now that we're idle so the screen retries for the correct timeframe.
    if (!pending_tf_.isEmpty()) {
        const QString tf = pending_tf_;
        pending_tf_.clear();
        emit timeframe_changed(tf);
    }
}

void CryptoChart::append_candle(const trading::Candle& candle) {
    // Reject malformed candles — zero/negative prices corrupt the axis range
    if (candle.open <= 0.0 || candle.high <= 0.0 || candle.low <= 0.0 || candle.close <= 0.0 || candle.timestamp <= 0)
        return;
    if (candle.low > candle.high)
        return;

    const auto sets = series_->sets();
    const bool is_update = !candles_.isEmpty() && candles_.last().timestamp == candle.timestamp;

    if (is_update) {
        const trading::Candle prev = candles_.last();
        candles_.last() = candle;
        if (!sets.isEmpty()) {
            auto* last_set = sets.last();
            last_set->setOpen(candle.open);
            last_set->setHigh(candle.high);
            last_set->setLow(candle.low);
            last_set->setClose(candle.close);
        }
        // Intrabar update: if the replaced candle defined the current extreme,
        // bounds may shrink — mark dirty. Otherwise cheap widen-only.
        if (!bounds_dirty_) {
            if (prev.high >= cached_max_price_ || prev.low <= cached_min_price_) {
                bounds_dirty_ = true;
            } else {
                cached_max_price_ = std::max(cached_max_price_, candle.high);
                cached_min_price_ = std::min(cached_min_price_, candle.low);
            }
        }
    } else {
        candles_.append(candle);
        if (candles_.size() > MAX_VISIBLE * 2) {
            candles_.remove(0, candles_.size() - MAX_VISIBLE);
            rebuild_chart();
            return;
        }
        if (sets.size() >= MAX_VISIBLE) {
            // Evicting the oldest set may drop the current extreme — mark dirty
            // so we rescan exactly once on the next axis update.
            const auto* oldest = sets.first();
            if (!bounds_dirty_ &&
                (oldest->low() <= cached_min_price_ || oldest->high() >= cached_max_price_)) {
                bounds_dirty_ = true;
            }
            series_->remove(sets.first());
        }
        series_->append(new QCandlestickSet(candle.open, candle.high, candle.low, candle.close, candle.timestamp));

        // Widen-only for new candle if bounds are clean.
        if (!bounds_dirty_) {
            cached_max_price_ = std::max(cached_max_price_, candle.high);
            cached_min_price_ = std::min(cached_min_price_, candle.low);
        }
    }

    const auto& visible_sets = series_->sets();
    if (visible_sets.isEmpty())
        return;

    if (bounds_dirty_)
        recompute_bounds();

    // Time axis — min is O(1) from sets.first, max from last. Sets stay ordered
    // by insertion which matches timestamp order for this use case.
    const qint64 min_time = static_cast<qint64>(visible_sets.first()->timestamp());
    const qint64 max_time = static_cast<qint64>(visible_sets.last()->timestamp());
    update_axes(cached_min_price_, cached_max_price_, min_time, max_time);
}

void CryptoChart::recompute_bounds() {
    const auto& sets = series_->sets();
    if (sets.isEmpty()) {
        cached_min_price_ = -1;
        cached_max_price_ = -1;
        bounds_dirty_ = false;
        return;
    }
    double mn = 1e18, mx = 0;
    for (const auto* s : sets) {
        mn = std::min(mn, s->low());
        mx = std::max(mx, s->high());
    }
    cached_min_price_ = mn;
    cached_max_price_ = mx;
    bounds_dirty_ = false;
}

void CryptoChart::clear() {
    candles_.clear();
    series_->clear();
    cached_min_price_ = cached_max_price_ = -1;
    bounds_dirty_ = true;
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

    // QCandlestickSeries computes bar width as bodyWidth * (axis_span / bar_count).
    // If min_time == max_time (single candle or all same timestamp), bar width → 0
    // and all candles render as hairlines. Add a one-interval padding on the right
    // so the axis always spans at least 2 × the per-candle slot.
    qint64 effective_max = max_time;
    if (min_time >= max_time) {
        // Fallback: extend by one slot using the TF label
        const QString tf = TF_LABELS[active_tf_];
        qint64 slot_ms = 60000; // 1m default
        if (tf == "5m")
            slot_ms = 300000;
        else if (tf == "15m")
            slot_ms = 900000;
        else if (tf == "1h")
            slot_ms = 3600000;
        else if (tf == "4h")
            slot_ms = 14400000;
        else if (tf == "1d")
            slot_ms = 86400000;
        effective_max = min_time + slot_ms;
    }

    if (min_time != last_min_time_ || effective_max != last_max_time_) {
        time_axis_->setRange(QDateTime::fromMSecsSinceEpoch(min_time), QDateTime::fromMSecsSinceEpoch(effective_max));
        last_min_time_ = min_time;
        last_max_time_ = effective_max;
    }
}

void CryptoChart::rebuild_chart() {
    series_->clear();
    last_min_price_ = last_max_price_ = -1;
    last_min_time_ = last_max_time_ = -1;
    cached_min_price_ = cached_max_price_ = -1;
    bounds_dirty_ = true;

    if (candles_.isEmpty())
        return;

    const int start = std::max(0, static_cast<int>(candles_.size()) - MAX_VISIBLE);
    double min_price = 1e18, max_price = 0;
    qint64 min_time = INT64_MAX, max_time = 0;

    for (int i = start; i < candles_.size(); ++i) {
        const auto& c = candles_[i];
        // Skip malformed candles — any zero/negative OHLC corrupts the axis range
        if (c.open <= 0.0 || c.high <= 0.0 || c.low <= 0.0 || c.close <= 0.0 || c.timestamp <= 0)
            continue;
        if (c.low > c.high || c.open > c.high || c.close > c.high)
            continue;
        series_->append(new QCandlestickSet(c.open, c.high, c.low, c.close, c.timestamp));
        min_price = std::min(min_price, c.low);
        max_price = std::max(max_price, c.high);
        if (c.timestamp < min_time)
            min_time = c.timestamp;
        if (c.timestamp > max_time)
            max_time = c.timestamp;
    }

    // Seed the incremental-bounds cache from this full scan.
    if (min_price < max_price) {
        cached_min_price_ = min_price;
        cached_max_price_ = max_price;
        bounds_dirty_ = false;
    }

    update_axes(min_price, max_price, min_time, max_time);
}

} // namespace fincept::screens::crypto
