#pragma once
// Equity Chart — Qt Charts candlestick chart with overlays.
// Upgraded from QPainter to share overlay infrastructure with CryptoChart.

#include "trading/TradingTypes.h"
#include "ui/charts/ChartOverlayManager.h"

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

namespace fincept::ui { class IndicatorPicker; }

namespace fincept::screens::equity {

class HoverEquityChartView;

class EquityChart : public QWidget {
    Q_OBJECT
public:
    explicit EquityChart(QWidget* parent = nullptr);

    void set_candles(const QVector<trading::BrokerCandle>& candles);
    void clear();

    QString current_timeframe() const;
    fincept::ui::ChartOverlayManager* overlay_manager() const { return overlay_mgr_; }

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

    HoverEquityChartView* chart_view_ = nullptr;
    QChart* chart_ = nullptr;
    QCandlestickSeries* series_ = nullptr;
    QDateTimeAxis* time_axis_ = nullptr;
    QValueAxis* price_axis_ = nullptr;
    QLineSeries* last_price_line_ = nullptr;

    QGraphicsLineItem* xhair_v_ = nullptr;
    QGraphicsLineItem* xhair_h_ = nullptr;
    QGraphicsEllipseItem* xhair_dot_ = nullptr;
    QGraphicsRectItem* price_tag_bg_ = nullptr;
    QGraphicsSimpleTextItem* price_tag_txt_ = nullptr;
    QGraphicsRectItem* time_tag_bg_ = nullptr;
    QGraphicsSimpleTextItem* time_tag_txt_ = nullptr;
    QGraphicsRectItem* last_tag_bg_ = nullptr;
    QGraphicsSimpleTextItem* last_tag_txt_ = nullptr;

    QLabel* ohlc_tooltip_ = nullptr;

    QPushButton* tf_buttons_[6] = {};
    int active_tf_ = 2; // default "15m"
    static constexpr const char* TF_LABELS[] = {"1m", "5m", "15m", "1h", "1d", "1w"};

    QVector<trading::BrokerCandle> candles_;
    static constexpr int MAX_VISIBLE = 120;

    double last_min_price_ = -1;
    double last_max_price_ = -1;
    qint64 last_min_time_ = -1;
    qint64 last_max_time_ = -1;
    double cached_min_price_ = -1;
    double cached_max_price_ = -1;
    bool bounds_dirty_ = true;
    void recompute_bounds();

    // Synthetic sequential timestamps — eliminates QDateTimeAxis gaps for
    // non-trading hours. Each visible candle gets evenly-spaced synthetic
    // timestamps; real timestamps are looked up from candles_ by index.
    int64_t synth_base_ts_ = 0;   // synthetic timestamp of first visible candle
    int64_t synth_slot_ms_ = 0;   // slot width in ms
    int display_start_ = 0;       // index into candles_ of first visible candle
    int display_count_ = 0;       // number of visible candles
    int64_t synth_ts_for(int candle_index) const;
    int candle_index_from_synth(int64_t synth_ts) const;
    const trading::BrokerCandle* candle_near_synth(int64_t synth_ts) const;

    fincept::ui::ChartOverlayManager* overlay_mgr_ = nullptr;
    fincept::ui::IndicatorPicker* indicator_picker_ = nullptr;

    friend class HoverEquityChartView;
};

} // namespace fincept::screens::equity
