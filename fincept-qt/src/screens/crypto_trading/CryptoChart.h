#pragma once
// Crypto Chart — Candlestick chart with timeframe toggles, crosshair,
// OHLC tooltip, and a last-price tag pinned to the right axis.

#include "trading/TradingTypes.h"

#include <QPushButton>
#include <QVector>
#include <QWidget>

class QChartView;
class QChart;
class QCandlestickSeries;
class QLineSeries;
class QDateTimeAxis;
class QValueAxis;
class QGraphicsLineItem;
class QGraphicsRectItem;
class QGraphicsEllipseItem;
class QGraphicsSimpleTextItem;
class QLabel;

namespace fincept::screens::crypto {

class HoverChartView; // forward; defined in the .cpp

class CryptoChart : public QWidget {
    Q_OBJECT
  public:
    explicit CryptoChart(QWidget* parent = nullptr);

    void set_candles(const QVector<trading::Candle>& candles);
    void append_candle(const trading::Candle& candle);
    void clear();

    QString current_timeframe() const;

  signals:
    void timeframe_changed(const QString& tf);

  private:
    void rebuild_chart();
    void update_axes(double min_price, double max_price, qint64 min_time, qint64 max_time);
    void set_active_tf(int idx);
    void apply_tf_axis_format();
    void update_last_price_marker();
    void on_hover_position(const QPointF& chart_value_pos, const QPoint& view_pos);
    void on_hover_leave();

    HoverChartView* chart_view_ = nullptr;
    QChart* chart_ = nullptr;
    QCandlestickSeries* series_ = nullptr;
    QDateTimeAxis* time_axis_ = nullptr;
    QValueAxis* price_axis_ = nullptr;
    QLineSeries* last_price_line_ = nullptr;

    // Crosshair / hover overlay (live in the chart's QGraphicsScene)
    QGraphicsLineItem* xhair_v_ = nullptr;
    QGraphicsLineItem* xhair_h_ = nullptr;
    QGraphicsEllipseItem* xhair_dot_ = nullptr;
    QGraphicsRectItem* price_tag_bg_ = nullptr;
    QGraphicsSimpleTextItem* price_tag_txt_ = nullptr;
    QGraphicsRectItem* time_tag_bg_ = nullptr;
    QGraphicsSimpleTextItem* time_tag_txt_ = nullptr;

    // Last-price tag (always-visible, tracks the latest close)
    QGraphicsRectItem* last_tag_bg_ = nullptr;
    QGraphicsSimpleTextItem* last_tag_txt_ = nullptr;

    // OHLC tooltip pinned to the chart's top-left corner
    QLabel* ohlc_tooltip_ = nullptr;

    // Timeframe toggle buttons
    QPushButton* tf_buttons_[6] = {};
    int active_tf_ = 3; // default "1h"
    static constexpr const char* TF_LABELS[] = {"1m", "5m", "15m", "1h", "4h", "1d"};

    QVector<trading::Candle> candles_;
    static constexpr int MAX_VISIBLE = 120;

    // Axis range cache (padded values actually applied to the axis)
    double last_min_price_ = -1;
    double last_max_price_ = -1;
    qint64 last_min_time_ = -1;
    qint64 last_max_time_ = -1;

    // Incremental price bounds over visible candle set — avoids O(N) rescans
    // on every intrabar tick. -1 means "stale, recompute on next append".
    double cached_min_price_ = -1;
    double cached_max_price_ = -1;
    bool bounds_dirty_ = true;
    void recompute_bounds();

    // Pending timeframe request while a fetch is already in-flight
    // set_candles() will emit timeframe_changed again if this is set
    QString pending_tf_;

    friend class HoverChartView;
};

} // namespace fincept::screens::crypto
